// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTENT_TEST_ASH_CONTENT_TEST_H_
#define ASH_SHELL_CONTENT_TEST_ASH_CONTENT_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/test/content_browser_test.h"

// A base class for ash test with content. The test will run with multi process
// with gpu/viz process, much closer to real environment than ash_unittests.
class AshContentTest : public content::ContentBrowserTest {
 public:
  AshContentTest();
  ~AshContentTest() override;

  // contnet::ContentBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 protected:
  aura::Window* CreateBrowserWindow(const GURL& url);
  aura::Window* CreateTestWindow();

  virtual std::vector<std::string> GetUMAHistogramNames() const;

  // Returns the names of timeline based metrics (TBM) to be extracted from
  // the generated trace. The metrics must be defined in telemetry
  //   third_party/catapult/tracing/tracing/metrics/
  // so that third_party/catapult/tracing/bin/run_metric could handle them.
  // Default is "renderingMetric", "umaMetric".
  virtual std::vector<std::string> GetTimelineBasedMetrics() const;

 private:
  class Tracer;

  gfx::Size test_window_size_;
  bool enable_trace_;
  std::unique_ptr<Tracer> tracer_;

  DISALLOW_COPY_AND_ASSIGN(AshContentTest);
};

#define ASH_CONTENT_TEST_F(test_fixture, test_name) \
  IN_PROC_BROWSER_TEST_F(test_fixture, test_name)
#define ASH_CONTENT_TEST_P(test_fixture, test_name) \
  IN_PROC_BROWSER_TEST_P(test_fixture, test_name)

#endif  // ASH_SHELL_CONTENT_TEST_ASH_CONTENT_TEST_H_
