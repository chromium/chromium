// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace printing {

// static
void TestPrintViewManagerForRequestPreview::CreateForWebContents(
    WebContents* web_contents) {
  web_contents->SetUserData(
      PrintViewManager::UserDataKey(),
      std::make_unique<TestPrintViewManagerForRequestPreview>(web_contents));
}

TestPrintViewManagerForRequestPreview::TestPrintViewManagerForRequestPreview(
    WebContents* web_contents)
    : PrintViewManager(web_contents) {}

TestPrintViewManagerForRequestPreview::
    ~TestPrintViewManagerForRequestPreview() = default;

// static
TestPrintViewManagerForRequestPreview*
TestPrintViewManagerForRequestPreview::FromWebContents(
    WebContents* web_contents) {
  return static_cast<TestPrintViewManagerForRequestPreview*>(
      PrintViewManager::FromWebContents(web_contents));
}

void TestPrintViewManagerForRequestPreview::set_quit_closure(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

void TestPrintViewManagerForRequestPreview::RequestPrintPreview(
    mojom::RequestPrintPreviewParamsPtr params) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(quit_closure_));
  PrintViewManager::RequestPrintPreview(std::move(params));
}

}  // namespace printing
