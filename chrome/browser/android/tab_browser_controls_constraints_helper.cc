// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/input/android/offset_tag_android.h"
#include "cc/input/browser_controls_animation_params.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/cubic_bezier.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabBrowserControlsConstraintsHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

static void JNI_TabBrowserControlsConstraintsHelper_UpdateState(
    JNIEnv* env,
    content::WebContents* web_contents,
    int32_t constraints,
    int32_t current,
    bool animate,
    const JavaRef<jobject>& joffset_tag_modifications) {
  cc::BrowserControlsState constraints_state =
      static_cast<cc::BrowserControlsState>(constraints);
  cc::BrowserControlsState current_state =
      static_cast<cc::BrowserControlsState>(current);

  if (web_contents == nullptr) {
    return;
  }

  cc::BrowserControlsOffsetTagModifications offset_tag_modifications =
      cc::android::FromJavaBrowserControlsOffsetTagModifications(
          env, joffset_tag_modifications);
  web_contents->UpdateBrowserControlsState(constraints_state, current_state,
                                           animate, offset_tag_modifications);
}

static void
JNI_TabBrowserControlsConstraintsHelper_StartBrowserControlsAnimation(
    JNIEnv* env,
    content::WebContents* web_contents,
    float target_top_controls_visible_height,
    float target_bottom_controls_visible_height,
    int64_t duration_ms,
    float curve_x1,
    float curve_y1,
    float curve_x2,
    float curve_y2) {
  CHECK(base::FeatureList::IsEnabled(::features::kControlsInBrowser));
  CHECK(std::isfinite(target_top_controls_visible_height));
  CHECK(std::isfinite(target_bottom_controls_visible_height));
  CHECK_GE(duration_ms, 0);
  CHECK(curve_x1 >= 0.f && curve_x1 <= 1.f);
  CHECK(std::isfinite(curve_y1));
  CHECK(curve_x2 >= 0.f && curve_x2 <= 1.f);
  CHECK(std::isfinite(curve_y2));

  if (web_contents == nullptr) {
    return;
  }

  web_contents->StartBrowserControlsAnimation(
      cc::BrowserControlsAnimationParams{
          .target_top_controls_visible_height =
              target_top_controls_visible_height,
          .target_bottom_controls_visible_height =
              target_bottom_controls_visible_height,
          .duration = base::Milliseconds(duration_ms),
          .curve = gfx::CubicBezier(curve_x1, curve_y1, curve_x2, curve_y2),
      });
}

DEFINE_JNI(TabBrowserControlsConstraintsHelper)
