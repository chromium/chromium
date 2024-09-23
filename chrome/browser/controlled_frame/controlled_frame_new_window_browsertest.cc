// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StartsWith;
using testing::UnorderedElementsAre;

namespace controlled_frame {

class ControlledFrameNewWindowBrowserTest
    : public ControlledFrameTestBase,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameTestBase::SetUpOnMainThread();
    StartContentServer("web_apps/simple_isolated_app");
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowBrowserTest, AttachSucceeds) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  auto test_script = content::JsReplace(
      R"(
(async function() {
  try {
    await new Promise((resolve, reject) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame) {
        reject('Could not find a controlledframe element.');
      }
      frame.addEventListener('newwindow', (e) => {
        if (!e.window.attach) {
          reject('window.attach does not exist ');
        }
        const newcontrolledframe = document.createElement('controlledframe');
        // Attach the new window to the new <controlledframe>.
        try {
          newcontrolledframe.addEventListener(
              'loadstop', resolve);
          newcontrolledframe.addEventListener(
              'loadabort', reject);
          e.window.attach(newcontrolledframe);
          document.body.appendChild(newcontrolledframe);
        } catch (err) {
          reject(err.message);
        }
      });
      frame.executeScript({code: 'window.open($1);'});
    });

    const frames = document.getElementsByTagName('controlledframe');
    if (frames.length !== 2) {
      return [
        'FAIL: expected 2 <controlledframe> elements, found ' + frames.length
      ];
    }

    async function getCurrentLocationOfControlledFrame(frame) {
      const result = await frame.executeScript({code: 'window.location.href;'});
      if (!result) {
        return 'FAIL: executeScript() returned no result';
      }
      return result[0];
    };

    return [
      await getCurrentLocationOfControlledFrame(frames[0]),
      await getCurrentLocationOfControlledFrame(frames[1]),
    ];
  } catch (e) {
    return ['FAIL: ' + e.message];
  }
})();
    )",
      embedded_https_test_server().GetURL("/index.html"));

  EXPECT_THAT(
      content::EvalJs(app_frame, test_script).ExtractList().GetList(),
      UnorderedElementsAre(
          embedded_https_test_server().GetURL("/controlled_frame.html").spec(),
          embedded_https_test_server().GetURL("/index.html").spec()));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowBrowserTest, DiscardSucceeds) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  std::string test_script =
      content::JsReplace(R"(
(async function() {
  try {
    return await new Promise ((resolve) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame) {
        resolve('FAIL: Could not find a controlledframe element.');
      }
      frame.addEventListener('newwindow', (e) => {
        try {
          e.window.discard();
          resolve('SUCCESS');
        } catch (err) {
          resolve('FAIL: ' + err.message);
        }
      });
      frame.executeScript({code: 'window.open($1);'});
    });
  } catch (err) {
    return "FAIL: " + err.message;
  }
})();
    )",
                         embedded_https_test_server().GetURL("/index.html"));

  ASSERT_EQ("SUCCESS", content::EvalJs(app_frame, test_script));

  EXPECT_EQ(1, content::EvalJs(
                   app_frame,
                   "document.getElementsByTagName('controlledframe').length;"));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameNewWindowBrowserTest,
                       PostMessageAfterAttachSucceeds) {
  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt,
          "/controlled_frame.html");

  auto test_script = content::JsReplace(
      R"(
async function executeScriptOnFrame(frame, script) {
  const result = await frame.executeScript({code: script});
  if (!result) {
    throw new Error('executeScript returned no result');
  }
  if (result[0] !== 'SUCCESS') {
    throw new Error('expected SUCCESS but got ' + result[0]);
  }
  return 'SUCCESS';
};

(async function() {
  try {
    await new Promise((resolve, reject) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame) {
        reject('Could not find a controlledframe element.');
      }

      frame.addEventListener('newwindow', (e) => {
        if (!e.window.attach) {
          reject('window.attach does not exist ');
        }
        const newcontrolledframe = document.createElement('controlledframe');
        // Attach the new window to the new <controlledframe>.
        try {
          newcontrolledframe.addEventListener('loadstop', resolve);
          newcontrolledframe.addEventListener('loadabort', reject);
          e.window.attach(newcontrolledframe);
          document.body.appendChild(newcontrolledframe);
        } catch (err) {
          reject(err.message);
        }
      });
      frame.executeScript({code: 'document.openedWindow = window.open($1);'});
    });

    const listenscript = `
      (function() {
        window.addEventListener('message', (e) => {document.lastMessage = e;});
        return 'SUCCESS';
      })();
    `;
    const sendscript = `
      (function() {
        const target = document.openedWindow || window.opener;
        if (target === null) {
          return 'missing postMessage target';
        }
        target.postMessage('hello test');
        return 'SUCCESS';
      })();
    `;
    const verifyscript = `
      (function() {
        if (!document.lastMessage) {
          return 'no message received';
        }
        if (document.lastMessage.data !== 'hello test') {
          return 'unexpected lastMessage\\nexpected: hello test\\nactual: ' +
              document.lastMessage.data;
        }
        return 'SUCCESS';
      })();
    `;

    const frames = Array.from(document.getElementsByTagName('controlledframe'));
    if (frames.length !== 2) {
      throw new Error(
          'expected 2 <controlledframe> elements, found ' + frames.length);
    }
    for (const frame of frames) {
      await executeScriptOnFrame(frame, listenscript);
    }
    for (const frame of frames) {
      await executeScriptOnFrame(frame, sendscript);
    }
    // Trigger resolve() in 0.1s after sending out messages.
    await new Promise((resolve) => {
      setTimeout(resolve, 100);
    });
    for (const frame of frames) {
      await executeScriptOnFrame(frame, verifyscript);
    }
    return 'SUCCESS';
  } catch (e) {
    return 'FAIL: ' + e.message;
  }
})();
    )",
      embedded_https_test_server().GetURL("/index.html"));

  EXPECT_EQ("SUCCESS", content::EvalJs(app_frame, test_script));
}

}  // namespace controlled_frame
