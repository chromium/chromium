// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_EXAMPLE_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_EXAMPLE_H_

#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

class VIEWS_EXAMPLES_EXPORT WatermarkExample
    : public views::examples::ExampleBase {
 public:
  WatermarkExample();
  WatermarkExample(const WatermarkExample&) = delete;
  WatermarkExample& operator=(const WatermarkExample&) = delete;
  ~WatermarkExample() override;

  // ExampleBase:
  void CreateExampleView(views::View* container) override;
};

#endif  // UI_VIEWS_EXAMPLES_WATERMARK_EXAMPLE_H_
