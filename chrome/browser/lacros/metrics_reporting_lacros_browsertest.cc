// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {
namespace {

class TestObserver : public mojom::MetricsReportingObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // crosapi::mojom::MetricsReportingObserver:
  void OnMetricsReportingChanged(bool enabled) override {
    metrics_enabled_ = enabled;
    if (on_changed_run_loop_)
      on_changed_run_loop_->Quit();
  }

  // Public because this is test code.
  base::Optional<bool> metrics_enabled_;
  base::RunLoop* on_changed_run_loop_ = nullptr;
  mojo::Receiver<mojom::MetricsReportingObserver> receiver_{this};
};

using MetricsReportingLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MetricsReportingLacrosBrowserTest, Basics) {
  auto* lacros_service = chromeos::LacrosChromeServiceImpl::Get();
  ASSERT_TRUE(lacros_service);

  // We don't assert the initial metrics state because it might vary depending
  // on the ash build type (official vs. not).
  const bool ash_metrics_enabled =
      lacros_service->init_params()->ash_metrics_enabled;

  mojo::Remote<mojom::MetricsReporting> metrics_reporting;
  lacros_service->BindMetricsReporting(
      metrics_reporting.BindNewPipeAndPassReceiver());

  // Adding an observer fires it.
  base::RunLoop run_loop1;
  TestObserver observer1;
  observer1.on_changed_run_loop_ = &run_loop1;
  metrics_reporting->AddObserver(
      observer1.receiver_.BindNewPipeAndPassRemote());
  run_loop1.Run();
  ASSERT_TRUE(observer1.metrics_enabled_.has_value());
  EXPECT_EQ(ash_metrics_enabled, observer1.metrics_enabled_.value());

  // Adding another observer fires it as well.
  base::RunLoop run_loop2;
  TestObserver observer2;
  observer2.on_changed_run_loop_ = &run_loop2;
  metrics_reporting->AddObserver(
      observer2.receiver_.BindNewPipeAndPassRemote());
  run_loop2.Run();
  ASSERT_TRUE(observer2.metrics_enabled_.has_value());
  EXPECT_EQ(ash_metrics_enabled, observer2.metrics_enabled_.value());

  // Exercise SetMetricsReportingEnabled() and ensure its callback is called.
  base::RunLoop run_loop3;
  metrics_reporting->SetMetricsReportingEnabled(!ash_metrics_enabled,
                                                run_loop3.QuitClosure());
  run_loop3.Run();

  base::RunLoop run_loop4;
  metrics_reporting->SetMetricsReportingEnabled(ash_metrics_enabled,
                                                run_loop4.QuitClosure());
  run_loop4.Run();
}

}  // namespace
}  // namespace crosapi
