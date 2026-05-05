// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_stability_test_util.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

PageStabilityTest::PageStabilityTest() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlic, features::kGlicActor},
      /*disabled_features=*/{features::kGlicWarming});
}

PageStabilityTest::~PageStabilityTest() = default;

void PageStabilityTest::SetUpOnMainThread() {
  PageStabilityBrowserTestBase::SetUpOnMainThread();
}

mojo::Remote<page_content_annotations::mojom::PageStabilityMonitor>
PageStabilityTest::CreatePageStabilityMonitor(bool supports_paint_stability) {
  // Actor tests specifically want to use the ChromeRenderFrame interface to
  // test the actor-specific monitor delegate.
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  mojo::Remote<page_content_annotations::mojom::PageStabilityMonitor>
      monitor_remote;
  chrome_render_frame->CreatePageStabilityMonitor(
      monitor_remote.BindNewPipeAndPassReceiver(), TaskId(),
      supports_paint_stability);

  // Ensure the monitor is created in the renderer before returning it.
  monitor_remote.FlushForTesting();

  return monitor_remote;
}

}  // namespace actor
