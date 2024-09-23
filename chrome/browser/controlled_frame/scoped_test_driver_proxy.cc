// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/scoped_test_driver_proxy.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"

namespace controlled_frame {
namespace {

void HandleSetPermission(content::TestDevToolsProtocolClient& devtools_client,
                         const base::Value::Dict& arguments) {
  const std::string* name = arguments.FindString("name");
  const std::string* state = arguments.FindString("state");
  const std::string* origin = arguments.FindString("origin");
  CHECK(name && state) << "Invalid set_permission arguments: " << arguments;

  base::Value::Dict args;
  args.Set("permission", base::Value::Dict().Set("name", *name));
  args.Set("setting", *state);
  if (origin) {
    args.Set("origin", *origin);
  }
  devtools_client.SendCommandSync("Browser.setPermission", std::move(args));
}

}  // namespace

ScopedTestDriverProxy::ScopedTestDriverProxy(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host), message_queue_(render_frame_host) {
  devtools_client_.AttachToWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host));
  HandleMessages();
}

ScopedTestDriverProxy::~ScopedTestDriverProxy() {
  devtools_client_.DetachProtocolClient();
}

// static
const char* ScopedTestDriverProxy::testdriver_override_script_src() {
  return R"(
    (function() {
      function sendMessage(actionName, arguments) {
        const nonce = `cb${Math.random().toString(16).slice(2)}`;
        return new Promise((resolve) => {
          window[nonce] = resolve;
          domAutomationController.send({
            action: actionName,
            callback: `window.${nonce}()`,
            arguments,
          });
        });
      }

      window.test_driver_internal.set_permission =
          function(permission_params, context) {
        return sendMessage('set_permission', {
          name: permission_params.descriptor.name,
          state: permission_params.state,
          origin: context?.origin
        });
      };
    })())";
}

void ScopedTestDriverProxy::HandleMessages() {
  while (message_queue_.HasMessages()) {
    std::string message_json;
    CHECK(message_queue_.PopMessage(&message_json));
    std::optional<base::Value::Dict> message =
        base::JSONReader::ReadDict(message_json);
    if (!message.has_value()) {
      LOG(ERROR) << "Invalid message from domAutomationController: "
                 << message_json;
      continue;
    }
    std::string* action = message->FindString("action");
    std::string* callback = message->FindString("callback");
    base::Value::Dict* arguments = message->FindDict("arguments");
    if (!action || !callback || !arguments) {
      LOG(ERROR) << "Invalid message from domAutomationController: "
                 << message_json;
      continue;
    }

    if (*action == "set_permission") {
      HandleSetPermission(devtools_client_, *arguments);
    }

    CHECK(content::ExecJs(render_frame_host_, *callback));
  }
  message_queue_.SetOnMessageAvailableCallback(base::BindOnce(
      &ScopedTestDriverProxy::HandleMessages, base::Unretained(this)));
}

}  // namespace controlled_frame
