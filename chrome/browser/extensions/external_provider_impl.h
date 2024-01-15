// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_IMPL_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_loader.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/common/manifest.h"

class Profile;

namespace base {
class Version;
}

namespace extensions {

// A specialization of the ExternalProvider that uses an instance of
// ExternalLoader to provide external extensions. This class can be seen as a
// bridge between the extension system and an ExternalLoader. Instances live
// their entire life on the UI thread.
class ExternalProviderImpl : public ExternalProviderInterface {
 public:
  // The constructed provider will provide the extensions loaded from |loader|
  // to |service|, that will deal with the installation. The location
  // attributes of the provided extensions are also specified here:
  // |crx_location|: extensions originating from crx files
  // |download_location|: extensions originating from update URLs
  // If either of the origins is not supported by this provider, then it should
  // be initialized as mojom::ManifestLocation::kInvalidLocation.
  ExternalProviderImpl(VisitorInterface* service,
                       const scoped_refptr<ExternalLoader>& loader,
                       Profile* profile,
                       mojom::ManifestLocation crx_location,
                       mojom::ManifestLocation download_location,
                       int creation_flags);

  ExternalProviderImpl(const ExternalProviderImpl&) = delete;
  ExternalProviderImpl& operator=(const ExternalProviderImpl&) = delete;

  ~ExternalProviderImpl() override;

  // Populates a list with providers for all known sources.
  static void CreateExternalProviders(
      VisitorInterface* service,
      Profile* profile,
      ProviderCollection* provider_list);

  // Sets underlying prefs and notifies provider. Only to be called by the
  // owned ExternalLoader instance.
  virtual void SetPrefs(base::Value::Dict prefs);

  // Updates the underlying prefs and notifies provider.
  // Only to be called by the owned ExternalLoader instance.
  void UpdatePrefs(base::Value::Dict prefs);

  // ExternalProvider implementation:
  void ServiceShutdown() override;
  void VisitRegisteredExtension() override;
  bool HasExtension(const std::string& id) const override;
  bool GetExtensionDetails(
      const std::string& id,
      mojom::ManifestLocation* location,
      std::unique_ptr<base::Version>* version) const override;

  bool IsReady() const override;
  void TriggerOnExternalExtensionFound() override;

  static const char kExternalCrx[];
  static const char kExternalVersion[];
  static const char kExternalUpdateUrl[];
  static const char kInstallParam[];
  static const char kIsBookmarkApp[];
  static const char kIsFromWebstore[];
  static const char kKeepIfPresent[];
  static const char kSupportedLocales[];
  static const char kWasInstalledByOem[];
  static const char kWebAppMigrationFlag[];
  static const char kMayBeUntrusted[];
  static const char kMinProfileCreatedByVersion[];
  static const char kDoNotInstallForEnterprise[];

  void set_auto_acknowledge(bool auto_acknowledge) {
    auto_acknowledge_ = auto_acknowledge;
  }

  void set_install_immediately(bool install_immediately) {
    install_immediately_ = install_immediately;
  }

  void set_allow_updates(bool allow_updates) { allow_updates_ = allow_updates; }

 private:
  bool HandleMinProfileVersion(const base::Value::Dict& extension,
                               const std::string& extension_id,
                               std::set<std::string>* unsupported_extensions);

  bool HandleDoNotInstallForEnterprise(
      const base::Value::Dict& extension,
      const std::string& extension_id,
      std::set<std::string>* unsupported_extensions);

  // Retrieves the extensions that were found in this provider.
  void RetrieveExtensionsFromPrefs(
      std::vector<ExternalInstallInfoUpdateUrl>* external_update_url_extensions,
      std::vector<ExternalInstallInfoFile>* external_file_extensions);

  // Retrieves the extensions from prefs and notifies the extension service for
  // each extension file/update URL found.
  void NotifyServiceOnExternalExtensionsFound();

  // Location for external extensions that are provided by this provider from
  // local crx files.
  const mojom::ManifestLocation crx_location_;

  // Location for external extensions that are provided by this provider from
  // update URLs.
  const mojom::ManifestLocation download_location_;

  // Weak pointer to the object that consumes the external extensions.
  // This is zeroed out by: ServiceShutdown()
  raw_ptr<VisitorInterface> service_;  // weak

  // Dict of the external extensions that are provided by this provider.
  std::optional<base::Value::Dict> prefs_;

  // Indicates that the extensions provided by this provider are loaded
  // entirely.
  bool ready_ = false;

  // The loader that loads the list of external extensions and reports them
  // via |SetPrefs|.
  scoped_refptr<ExternalLoader> loader_;

  // The profile that will be used to install external extensions.
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extension::Create() by the crx installer.
  int creation_flags_;

  // Whether loaded extensions should be automatically acknowledged, so that
  // the user doesn't see an alert about them.
  bool auto_acknowledge_ = false;

  // Whether the extensions from this provider should be installed immediately.
  bool install_immediately_ = false;

  // Whether the provider should be allowed to update the set of external
  // extensions it provides.
  bool allow_updates_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_IMPL_H_
