// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsUIBindingsTest : public testing::Test {
};

TEST_F(DevToolsUIBindingsTest, SanitizeFrontendURL) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"random-string", "devtools://devtools/"},
      {"http://valid.url/but/wrong", "devtools://devtools/but/wrong"},
      {"devtools://wrong-domain/", "devtools://devtools/"},
      {"devtools://devtools/bundled/devtools.html",
       "devtools://devtools/bundled/devtools.html"},
      {"devtools://devtools:1234/bundled/devtools.html#hash",
       "devtools://devtools/bundled/devtools.html#hash"},
      {"devtools://devtools/some/random/path",
       "devtools://devtools/some/random/path"},
      {"devtools://devtools/bundled/devtools.html?experiments=true",
       "devtools://devtools/bundled/devtools.html?experiments=true"},
      {"devtools://devtools/bundled/devtools.html"
       "?some-flag=flag&v8only=true&experiments=false&debugFrontend=a"
       "&another-flag=another-flag&can_dock=false&isSharedWorker=notreally"
       "&remoteFrontend=sure",
       "devtools://devtools/bundled/devtools.html"
       "?v8only=true&experiments=true&debugFrontend=true"
       "&can_dock=true&isSharedWorker=true&remoteFrontend=true"},
      {"devtools://devtools/?ws=any-value-is-fine",
       "devtools://devtools/?ws=any-value-is-fine"},
      {"devtools://devtools/"
       "?service-backend=ws://localhost:9222/services",
       "devtools://devtools/"
       "?service-backend=ws://localhost:9222/services"},
      {"devtools://devtools/?dockSide=undocked",
       "devtools://devtools/?dockSide=undocked"},
      {"devtools://devtools/?dockSide=dock-to-bottom", "devtools://devtools/"},
      {"devtools://devtools/?dockSide=bottom", "devtools://devtools/"},
      {"devtools://devtools/?remoteBase="
       "http://example.com:1234/remote-base#hash",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/"
       "serve_file//#hash"},
      {"devtools://devtools/?ws=1%26evil%3dtrue",
       "devtools://devtools/?ws=1%26evil%3dtrue"},
      {"devtools://devtools/?ws=encoded-ok'",
       "devtools://devtools/?ws=encoded-ok%27"},
      {"devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/some/path/"
       "@123719741873/more/path.html",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/path/"},
      {"devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@123719741873/inspector.html%3FdebugFrontend%3Dfalse",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@123719741873/"},
      {"devtools://devtools/bundled/inspector.html?"
       "&remoteBase=https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@b4907cc5d602ff470740b2eb6344b517edecb7b9/&can_dock=true",
       "devtools://devtools/bundled/inspector.html?"
       "remoteBase=https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@b4907cc5d602ff470740b2eb6344b517edecb7b9/&can_dock=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%3FdebugFrontend%3Dfalse",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html%3FdebugFrontend%3Dtrue"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%22></iframe>something",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html"},
      {"devtools://devtools/?remoteFrontendUrl="
       "http://domain:1234/path/rev/a/filename.html%3Fparam%3Dvalue#hash",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2Frev%2Finspector.html#hash"},
      {"devtools://devtools/?experiments=whatever&remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/devtools.html%3Fws%3Danyvalue%26experiments%3Dlikely"
       "&unencoded=value&debugFrontend=true",
       "devtools://devtools/?experiments=true&remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Fdevtools.html%3Fws%3Danyvalue%26experiments%3Dtrue"
       "&debugFrontend=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%23%27",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html"},
      {"devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool",
       "devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool"},
      {"devtools://devtools/?enabledExperiments=invalidExperiment$",
       "devtools://devtools/"},
  };

  for (const auto& pair : tests) {
    GURL url = GURL(pair.first);
    url = DevToolsUIBindings::SanitizeFrontendURL(url);
    EXPECT_EQ(pair.second, url.spec());
  }
}
