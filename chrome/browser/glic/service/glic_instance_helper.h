// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace glic {

class GlicInstanceHelperMetrics;
enum class DaisyChainFirstAction;

// Attaches a InstanceId to a TabInterface. An instance of this class is
// created by and owned by TabFeatures.
class GlicInstanceHelper {
 public:
  DECLARE_USER_DATA(GlicInstanceHelper);

  static GlicInstanceHelper* From(tabs::TabInterface* tab);

  // Interface for the GlicInstance that interacts with this helper.
  class Instance {
   public:
    virtual const InstanceId& id() const = 0;
    virtual std::optional<std::string> conversation_id() const = 0;
    virtual std::string conversation_title() const = 0;
    virtual std::optional<mojom::InvocationSource> initial_invocation_source()
        const = 0;
  };

  explicit GlicInstanceHelper(tabs::TabInterface* tab);
  ~GlicInstanceHelper();

  std::optional<InstanceId> GetInstanceId() const;
  void SetBoundInstance(Instance* instance);

  std::optional<std::string> GetConversationId() const;
  std::string GetConversationTitle() const;
  std::optional<mojom::InvocationSource> GetInitialInvocationSource() const;

  void OnPinnedByInstance(Instance* instance);
  void OnUnpinnedByInstance(Instance* instance);

  std::vector<Instance*> GetPinnedInstances() const;

  void SetIsDaisyChained(DaisyChainSource source);
  void OnDaisyChainAction(DaisyChainFirstAction action);

  base::CallbackListSubscription SubscribeToDestruction(
      base::RepeatingCallback<void(tabs::TabInterface*)> callback);

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void OnConversationTitleChanged();
#endif

 private:
  raw_ptr<Instance> bound_instance_ = nullptr;
  base::flat_set<raw_ptr<Instance>> pinned_instances_;
  raw_ptr<tabs::TabInterface> tab_;
  std::unique_ptr<GlicInstanceHelperMetrics> metrics_;
  ui::ScopedUnownedUserData<GlicInstanceHelper> scoped_unowned_user_data_;
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      on_destroy_callback_list_;

#if BUILDFLAG(IS_ANDROID)
  void InitJavaObject();
  void NotifyJavaInstanceTitleChanged();

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_HELPER_H_
