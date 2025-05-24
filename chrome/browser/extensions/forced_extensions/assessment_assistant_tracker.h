// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_ASSESSMENT_ASSISTANT_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_ASSESSMENT_ASSISTANT_TRACKER_H_

#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class AssessmentAssistantTracker : public InstallStageTracker::Observer,
                                   public ExtensionRegistryObserver,
                                   public KeyedService {
 public:
  explicit AssessmentAssistantTracker(content::BrowserContext* context);
  ~AssessmentAssistantTracker() override;

  AssessmentAssistantTracker(const AssessmentAssistantTracker&) = delete;
  AssessmentAssistantTracker& operator=(const AssessmentAssistantTracker&) =
      delete;

  // InstallStageTracker::Observer overrides.
  void OnExtensionInstallationFailed(
      const ExtensionId& id,
      InstallStageTracker::FailureReason reason) override;
  void OnExtensionDownloadCacheStatusRetrieved(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status) override;
  void OnExtensionInstallationStageChanged(
      const ExtensionId& id,
      InstallStageTracker::Stage stage) override;
  void OnExtensionDownloadingStageChanged(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::Stage stage) override;
  void OnExtensionInstallCreationStageChanged(
      const ExtensionId& id,
      InstallStageTracker::InstallCreationStage stage) override;

  // ExtensionRegistryObserver overrides.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;
  void OnExtensionUninstallationDenied(content::BrowserContext* browser_context,
                                       const Extension* extension) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // KeyedService overrides.
  void Shutdown() override;

 private:
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  base::ScopedObservation<InstallStageTracker, InstallStageTracker::Observer>
      install_observation_{this};
};

class AssessmentAssistantTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AssessmentAssistantTracker* GetForBrowserContext(
      content::BrowserContext* context);

  static AssessmentAssistantTrackerFactory* GetInstance();

  AssessmentAssistantTrackerFactory(const AssessmentAssistantTrackerFactory&) =
      delete;
  AssessmentAssistantTrackerFactory& operator=(
      const AssessmentAssistantTrackerFactory&) = delete;

 private:
  friend class base::NoDestructor<AssessmentAssistantTrackerFactory>;
  AssessmentAssistantTrackerFactory();
  ~AssessmentAssistantTrackerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_ASSESSMENT_ASSISTANT_TRACKER_H_
