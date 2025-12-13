// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/assessment_assistant_tracker.h"

#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {
inline constexpr char kAssessmentAssistantExtensionId[] =
    "gndmhdcefbhlchkhipcnnbkcmicncehk";
}

namespace extensions {

AssessmentAssistantTracker::AssessmentAssistantTracker(
    content::BrowserContext* context) {
  auto* extension = ExtensionRegistry::Get(context)->GetInstalledExtension(
      kAssessmentAssistantExtensionId);

  if (extension) {
    LOG(WARNING) << "AssessmentAssistantTracker started for "
                    "AssessmentAssistant version "
                 << extension->version().GetString() << ".";
  } else {
    LOG(WARNING) << "AssessmentAssistantTracker started, but no "
                    "AssessmentAssistant is installed.";
  }

  registry_observation_.Observe(ExtensionRegistry::Get(context));
  install_observation_.Observe(
      InstallStageTrackerFactory::GetForBrowserContext(context));
}

AssessmentAssistantTracker::~AssessmentAssistantTracker() = default;

void AssessmentAssistantTracker::Shutdown() {
  if (!extensions::ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    LOG(WARNING) << "AssessmentAssistantTracker shutdown.";
  }
}

void AssessmentAssistantTracker::OnExtensionInstallationFailed(
    const ExtensionId& id,
    InstallStageTracker::FailureReason reason) {
  if (id == kAssessmentAssistantExtensionId) {
    LOG(WARNING)
        << "AssessmentAssistantTracker - installation failed with reason "
        << static_cast<int>(reason) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::CacheStatus cache_status) {
  if (id == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - cache status "
                 << static_cast<int>(cache_status) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionInstallationStageChanged(
    const ExtensionId& id,
    InstallStageTracker::Stage stage) {
  if (id == kAssessmentAssistantExtensionId) {
    LOG(WARNING)
        << "AssessmentAssistantTracker - installation stage changed to "
        << static_cast<int>(stage) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionDownloadingStageChanged(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::Stage stage) {
  if (id == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - download stage changed to "
                 << static_cast<int>(stage) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionInstallCreationStageChanged(
    const ExtensionId& id,
    InstallStageTracker::InstallCreationStage stage) {
  if (id == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - creation stage changed to "
                 << static_cast<int>(stage) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - extension loaded.";
  }
}

void AssessmentAssistantTracker::OnExtensionReady(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - extension ready.";
  }
}

void AssessmentAssistantTracker::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING)
        << "AssessmentAssistantTracker - extension unloaded with reason "
        << static_cast<int>(reason) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - extension will be "
                 << (is_update ? "updated." : "installed.");
  }
}

void AssessmentAssistantTracker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING) << "AssessmentAssistantTracker - extension "
                 << (is_update ? "updated." : "installed.");
  }
}

void AssessmentAssistantTracker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING)
        << "AssessmentAssistantTracker - extension uninstalled with reason "
        << static_cast<int>(reason) << ".";
  }
}

void AssessmentAssistantTracker::OnExtensionUninstallationDenied(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == kAssessmentAssistantExtensionId) {
    LOG(WARNING)
        << "AssessmentAssistantTracker - extension uninstallation denied.";
  }
}

void AssessmentAssistantTracker::OnShutdown(ExtensionRegistry* registry) {
  if (!extensions::ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    LOG(WARNING) << "AssessmentAssistantTracker - shutting down.";
  }
}

AssessmentAssistantTrackerFactory::AssessmentAssistantTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "AssessmentAssistantTracker",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(InstallStageTrackerFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

AssessmentAssistantTrackerFactory::~AssessmentAssistantTrackerFactory() =
    default;

std::unique_ptr<KeyedService>
AssessmentAssistantTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto tracker = std::make_unique<AssessmentAssistantTracker>((context));
  return tracker;
}

// static
AssessmentAssistantTracker*
AssessmentAssistantTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AssessmentAssistantTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AssessmentAssistantTrackerFactory*
AssessmentAssistantTrackerFactory::GetInstance() {
  static base::NoDestructor<AssessmentAssistantTrackerFactory> instance;
  return instance.get();
}

}  // namespace extensions
