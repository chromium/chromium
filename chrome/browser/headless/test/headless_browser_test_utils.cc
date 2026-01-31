// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/test/headless_browser_test_utils.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace headless {

base::DictValue SendCommandSync(SimpleDevToolsProtocolClient& devtools_client,
                                const std::string& command) {
  return SendCommandSync(devtools_client, command, base::DictValue());
}

base::DictValue SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command,
    base::DictValue params) {
  base::DictValue command_result;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  devtools_client.SendCommand(
      command, std::move(params),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::DictValue* command_result,
             base::DictValue result) {
            *command_result = std::move(result);
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&command_result)));
  run_loop.Run();

  return command_result;
}

}  // namespace headless
