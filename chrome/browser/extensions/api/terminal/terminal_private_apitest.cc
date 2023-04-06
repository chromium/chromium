// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/process_proxy/process_proxy_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"

namespace {

// For running and maintaining a `cat` process using `ProcessProxyRegistry`
// directly.
class CatProcess {
 public:
  CatProcess() {
    base::RunLoop run_loop;

    chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          chromeos::ProcessProxyRegistry* registry =
              chromeos::ProcessProxyRegistry::Get();

          auto on_output_on_process_thread = base::BindLambdaForTesting(
              [this](const std::string&, const std::string&,
                     const std::string&) {
                content::GetUIThreadTaskRunner({})->PostTask(
                    FROM_HERE, base::BindOnce(&CatProcess::OnOutputOnUIThread,
                                              base::Unretained(this)));
              });

          // `user_id_hash` does not seems to matter in test.
          this->ok_ = registry->OpenProcess(
              base::CommandLine(base::FilePath("cat")), /*user_id_hash=*/"user",
              on_output_on_process_thread, &this->process_id_);

          run_loop.Quit();
        }));

    run_loop.Run();
  }

  ~CatProcess() {
    if (ok_) {
      chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](const std::string& process_id) {
                chromeos::ProcessProxyRegistry::Get()->CloseProcess(process_id);
              },
              process_id_));
    }
  }

  bool ok() const { return ok_; }
  bool has_output() const { return has_output_; }
  const std::string& process_id() const { return process_id_; }

  void send_input_and_wait_output(const std::string& data) {
    base::RunLoop run_loop;
    CHECK(on_output_closure_.is_null());
    on_output_closure_ = run_loop.QuitClosure();

    chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          chromeos::ProcessProxyRegistry::Get()->SendInput(
              this->process_id_, data, base::DoNothing());
        }));

    run_loop.Run();
  }

 private:
  void OnOutputOnUIThread() {
    // We don't bother to call `ProcessProxyRegistry::AckOutput()` here since we
    // only need to know whether there is some output.
    has_output_ = true;

    if (!on_output_closure_.is_null()) {
      std::move(on_output_closure_).Run();
    }
  }

  bool ok_;
  std::string process_id_;
  bool has_output_{false};
  base::OnceClosure on_output_closure_;
};

}  // namespace

class ExtensionTerminalPrivateApiTest : public extensions::ExtensionApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "kidcpjlbjdmcnmccjhjdckhbngnhnepk");
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionTerminalPrivateApiTest, CatProcess) {
  CatProcess cat_process;
  ASSERT_TRUE(cat_process.ok());
  ASSERT_FALSE(cat_process.has_output());
  cat_process.send_input_and_wait_output("hello");
  ASSERT_TRUE(cat_process.has_output());
}

IN_PROC_BROWSER_TEST_F(ExtensionTerminalPrivateApiTest, TerminalTest) {
  CatProcess cat_process;
  ASSERT_TRUE(cat_process.ok());

  const std::string extension_url =
      "test.html?foreign_id=" + cat_process.process_id();
  EXPECT_TRUE(RunExtensionTest("terminal/component_extension",
                               {.extension_url = extension_url.c_str()}))
      << message_;

  // Double check that test.html cannot write to the cat process here;
  // Otherwises, we should detect some output.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cat_process.has_output());
}
