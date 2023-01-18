// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/test/headless_browser_test_utils.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace headless {

base::Value::Dict SendCommandSync(SimpleDevToolsProtocolClient& devtools_client,
                                  const std::string& command) {
  return SendCommandSync(devtools_client, command, base::Value::Dict());
}

base::Value::Dict SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command,
    base::Value::Dict params) {
  base::Value::Dict command_result;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  devtools_client.SendCommand(
      command, std::move(params),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::Value::Dict* command_result,
             base::Value::Dict result) {
            *command_result = std::move(result);
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&command_result)));
  run_loop.Run();

  return command_result;
}

}  // namespace headless
