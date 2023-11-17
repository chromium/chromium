// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "chrome/browser/chromeos/enterprise/cloud_storage/one_drive_pref_observer.h"

#include "base/containers/contains.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "content/public/test/browser_test.h"

namespace chromeos::cloud_storage {

class OneDrivePrefObserverBrowserTest : public InProcessBrowserTest {
 public:
  OneDrivePrefObserverBrowserTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kMicrosoftOneDriveIntegrationForEnterprise},
        {});
  }
  ~OneDrivePrefObserverBrowserTest() override = default;

 protected:
  bool OneDrivePrefObserverServiceExists() {
    std::vector<DependencyNode*> nodes;
    const bool success = BrowserContextDependencyManager::GetInstance()
                             ->GetDependencyGraphForTesting()
                             .GetConstructionOrder(&nodes);
    EXPECT_TRUE(success);
    return base::Contains(
        nodes, "OneDrivePrefObserverFactory",
        [](const DependencyNode* node) -> std::string_view {
          return static_cast<const KeyedServiceBaseFactory*>(node)->name();
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       KeyedServiceRegistered) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ASSERT_TRUE(OneDrivePrefObserverServiceExists());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_NE(crosapi::browser_util::IsLacrosEnabled(),
            OneDrivePrefObserverServiceExists());
#else
  NOTREACHED();
#endif
}

}  // namespace chromeos::cloud_storage
