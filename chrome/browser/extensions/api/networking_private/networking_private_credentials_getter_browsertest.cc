// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/services/wifi_util_win/public/mojom/wifi_credentials_getter.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_WIN)
#error This test is OS_WIN only.
#endif

class NetworkingPrivateCredentialsGetterTest : public InProcessBrowserTest {
 public:
  NetworkingPrivateCredentialsGetterTest() = default;

  void RunTest(bool use_test_network) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();

    if (use_test_network)
      network_ = chrome::mojom::WiFiCredentialsGetter::kWiFiTestNetwork;

    done_called_ = false;
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(&NetworkingPrivateCredentialsGetterTest::GetCredentials,
                   base::Unretained(this)));
    run_loop.Run();

    EXPECT_TRUE(done_called_);
  }

 private:
  void GetCredentials() {
    std::unique_ptr<extensions::NetworkingPrivateCredentialsGetter> getter(
        extensions::NetworkingPrivateCredentialsGetter::Create());
    getter->Start(
        network_, "public_key",
        base::Bind(&NetworkingPrivateCredentialsGetterTest::CredentialsDone,
                   base::Unretained(this)));
  }

  void CredentialsDone(const std::string& key_data, const std::string& error) {
    done_called_ = true;

    if (!network_.empty()) {
      EXPECT_EQ(network_, key_data);
      EXPECT_EQ("", error);
    } else {
      EXPECT_EQ("", key_data);
      EXPECT_FALSE(error.empty());
    }

    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             quit_closure_);
  }

  base::Closure quit_closure_;
  std::string network_;
  bool done_called_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateCredentialsGetterTest);
};

IN_PROC_BROWSER_TEST_F(NetworkingPrivateCredentialsGetterTest,
                       GetCredentialsSuccess) {
  RunTest(true);
}

IN_PROC_BROWSER_TEST_F(NetworkingPrivateCredentialsGetterTest,
                       GetCredentialsFailure) {
  RunTest(false);
}
