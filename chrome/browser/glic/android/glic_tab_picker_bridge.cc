// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_tab_picker_bridge.h"

#include <optional>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/jni_zero/default_conversions.h"
#include "ui/android/window_android.h"

namespace jni_zero {
template <>
inline std::vector<TabAndroid*> FromJniType<std::vector<TabAndroid*>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  if (!j_object) {
    return {};
  }
  return FromJniCollection<std::vector<TabAndroid*>>(env, j_object);
}
}  // namespace jni_zero

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicTabPickerBridge_jni.h"

namespace glic {

namespace {

void OnTabPickerCompleted(
    const std::vector<TabAndroid*>& initial_selected,
    base::WeakPtr<GlicSharingManager> sharing_manager,
    GlicTabPickerBridge::OnCompleteCallback on_complete_callback,
    std::optional<std::vector<TabAndroid*>> final_selected) {
  if (sharing_manager && final_selected.has_value()) {
    base::flat_set<TabAndroid*> initial_set(initial_selected);
    base::flat_set<TabAndroid*> final_set(final_selected.value());

    std::vector<tabs::TabHandle> tabs_to_pin;
    for (TabAndroid* tab : final_selected.value()) {
      if (tab && !initial_set.contains(tab)) {
        tabs_to_pin.push_back(tab->GetHandle());
      }
    }

    std::vector<tabs::TabHandle> tabs_to_unpin;
    for (TabAndroid* tab : initial_selected) {
      if (tab && !final_set.contains(tab)) {
        tabs_to_unpin.push_back(tab->GetHandle());
      }
    }

    if (!tabs_to_pin.empty()) {
      sharing_manager->PinTabs(tabs_to_pin, GlicPinTrigger::kTabPicker);
    }
    if (!tabs_to_unpin.empty()) {
      sharing_manager->UnpinTabs(tabs_to_unpin, GlicUnpinTrigger::kTabPicker);
    }
  }

  std::move(on_complete_callback).Run();
}

}  // namespace

void GlicTabPickerBridge::OpenTabPicker(
    ui::WindowAndroid* window_android,
    base::WeakPtr<GlicSharingManager> sharing_manager,
    OnCompleteCallback on_complete_callback) {
  if (!window_android) {
    std::move(on_complete_callback).Run();
    return;
  }

  std::vector<TabAndroid*> already_selected_tabs;
  if (sharing_manager) {
    for (tabs::TabInterface* tab : sharing_manager->GetPinnedTabs()) {
      if (tab) {
        if (TabAndroid* tab_android =
                TabAndroid::FromTabHandle(tab->GetHandle())) {
          already_selected_tabs.push_back(tab_android);
        }
      }
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  auto on_picker_done =
      base::BindOnce(&OnTabPickerCompleted, already_selected_tabs,
                     sharing_manager, std::move(on_complete_callback));

  Java_GlicTabPickerBridge_openTabPicker(
      env, window_android, already_selected_tabs, std::move(on_picker_done));
}

void GlicTabPickerBridge::OnTabPickerCompletedForTesting(  // IN-TEST
    const std::vector<TabAndroid*>& initial_selected,
    base::WeakPtr<GlicSharingManager> sharing_manager,
    OnCompleteCallback on_complete_callback,
    std::optional<std::vector<TabAndroid*>> final_selected) {
  OnTabPickerCompleted(initial_selected, sharing_manager,
                       std::move(on_complete_callback), final_selected);
}

}  // namespace glic
