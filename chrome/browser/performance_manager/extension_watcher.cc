// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/extension_watcher.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"

namespace performance_manager {

namespace {

// Values reported to the Extensions.BackgroundHostCreatedForExtension
// histogram.
enum class BackgroundHostCreatedForExtensionValue {
  // Any extension not listed below.
  kOther = 0,
  // Google Docs Offline (ghbmnnjooekpmoecnnnilnnbdlolhkhi)
  kDocsOffline = 1,
  // In-App Payment Support (nmmhkkegccagdldgiimedpiccmgmieda)
  kInAppPaymentSupport = 2,
  // Assessment Assistant (gndmhdcefbhlchkhipcnnbkcmicncehk)
  kAssessmentAssistant = 3,
  kMaxValue = kAssessmentAssistant
};

void RecordBackgroundHostCreatedForExtension(
    const extensions::ExtensionId& id) {
  BackgroundHostCreatedForExtensionValue value =
      BackgroundHostCreatedForExtensionValue::kOther;
  if (id == extension_misc::kDocsOfflineExtensionId) {
    value = BackgroundHostCreatedForExtensionValue::kDocsOffline;
  } else if (id == extension_misc::kInAppPaymentsSupportAppId) {
    value = BackgroundHostCreatedForExtensionValue::kInAppPaymentSupport;
#if BUILDFLAG(IS_CHROMEOS)
  } else if (id == extension_misc::kAssessmentAssistantExtensionId) {
    value = BackgroundHostCreatedForExtensionValue::kAssessmentAssistant;
#endif
  }

  base::UmaHistogramEnumeration("Extensions.BackgroundHostCreatedForExtension",
                                value);
}

}  // namespace

ExtensionWatcher::ExtensionWatcher() {
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
}

ExtensionWatcher::~ExtensionWatcher() = default;

void ExtensionWatcher::OnProfileAdded(Profile* profile) {
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    return;
  }

  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile);
  DCHECK(process_manager);
  extension_process_manager_observation_.AddObservation(process_manager);
}

void ExtensionWatcher::OnBackgroundHostCreated(
    extensions::ExtensionHost* host) {
  DCHECK_EQ(host->extension_host_type(),
            extensions::mojom::ViewType::kExtensionBackgroundPage);
  auto* registry = PerformanceManagerRegistry::GetInstance();
  DCHECK(registry);
  registry->SetPageType(host->host_contents(), PageType::kExtension);
  RecordBackgroundHostCreatedForExtension(host->extension_id());
}

void ExtensionWatcher::OnProcessManagerShutdown(
    extensions::ProcessManager* manager) {
  extension_process_manager_observation_.RemoveObservation(manager);
}

}  // namespace performance_manager
