// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/error_map.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionPrefs;

// The ErrorConsole is a central object to which all extension errors are
// reported. This includes errors detected in extensions core, as well as
// runtime Javascript errors. If FeatureSwitch::error_console() is enabled these
// errors can be viewed at chrome://extensions in developer mode.
// This class is owned by ExtensionSystem, making it, in effect, a
// BrowserContext-keyed service.
class ErrorConsole : public KeyedService,
                     public ProfileObserver,
                     public ExtensionRegistryObserver {
 public:
  class Observer {
   public:
    // Sent when a new error is reported to the error console.
    virtual void OnErrorAdded(const ExtensionError* error);

    // Sent when errors are removed from the error console. |extension_ids| is
    // the set of ids that were affected.
    // Note: This is not sent when an extension is uninstalled, or when a
    // profile is destroyed.
    virtual void OnErrorsRemoved(const std::set<std::string>& extension_ids);

    // Sent upon destruction to allow any observers to invalidate any references
    // they have to the error console.
    virtual void OnErrorConsoleDestroyed();
  };

  explicit ErrorConsole(Profile* profile);
  ~ErrorConsole() override;

  // Convenience method to return the ErrorConsole for a given |context|.
  static ErrorConsole* Get(content::BrowserContext* context);

  // Set whether or not errors of the specified |type| are stored for the
  // extension with the given |extension_id|. This will be stored in the
  // preferences.
  void SetReportingForExtension(const std::string& extension_id,
                                ExtensionError::Type type,
                                bool enabled);

  // Set whether or not errors of all types are stored for the extension with
  // the given |extension_id|.
  void SetReportingAllForExtension(const std::string& extension_id,
                                           bool enabled);

  // Returns true if reporting for either manifest or runtime errors is enabled
  // for the extension with the given |extension_id|.
  bool IsReportingEnabledForExtension(const std::string& extension_id) const;

  // Restore default reporting to the given extension.
  void UseDefaultReportingForExtension(const std::string& extension_id);

  // Report an extension error, and add it to the list.
  void ReportError(std::unique_ptr<ExtensionError> error);

  // Removes errors from the map according to the given |filter|.
  void RemoveErrors(const ErrorMap::Filter& filter);

  // Get a collection of weak pointers to all errors relating to the extension
  // with the given |extension_id|.
  const ErrorList& GetErrorsForExtension(const std::string& extension_id) const;

  // Add or remove observers of the ErrorConsole to be notified of any errors
  // added.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether or not the ErrorConsole is enabled for the
  // chrome:extensions page or the Chrome Apps Developer Tools.
  //
  // TODO(rdevlin.cronin): These have different answers - ErrorConsole is
  // enabled by default in ADT, but only Dev Channel for chrome:extensions (or
  // with the commandline switch). Once we do a full launch, clean all this up.
  bool IsEnabledForChromeExtensionsPage() const;
  bool IsEnabledForAppsDeveloperTools() const;

  // Return whether or not the ErrorConsole is enabled.
  bool enabled() const { return enabled_; }

  // Return the number of entries (extensions) in the error map.
  size_t get_num_entries_for_test() const { return errors_.size(); }

  // Set the default reporting for all extensions.
  void set_default_reporting_for_test(ExtensionError::Type type, bool enabled) {
    default_mask_ =
        enabled ? default_mask_ | (1 << type) : default_mask_ & ~(1 << type);
  }

 private:
  // Checks whether or not the ErrorConsole should be enabled or disabled. If it
  // is in the wrong state, enables or disables it appropriately.
  void CheckEnabled();

  // Enable the error console for error collection and retention. This involves
  // subscribing to the appropriate notifications and fetching manifest errors.
  void Enable();

  // Disable the error console, removing the subscriptions to notifications and
  // removing all current errors.
  void Disable();

  // Called when the Developer Mode preference is changed; this is important
  // since we use this as a heuristic to determine if the console is enabled or
  // not.
  void OnPrefChanged();

  // ExtensionRegistry implementation. If the Apps Developer Tools app is
  // installed or uninstalled, we may need to turn the ErrorConsole on/off.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // Add manifest errors from an extension's install warnings.
  void AddManifestErrorsForExtension(const Extension* extension);

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Returns the applicable bit mask of reporting preferences for the extension.
  int GetMaskForExtension(const std::string& extension_id) const;

  // Whether or not the error console should record errors. This is true if
  // the user is in developer mode, and at least one of the following is true:
  // - The Chrome Apps Developer Tools are installed.
  // - FeatureSwitch::error_console() is enabled.
  // - This is a Dev Channel release.
  bool enabled_;

  // Needed because base::ObserverList is not thread-safe.
  base::ThreadChecker thread_checker_;

  // The list of all observers for the ErrorConsole.
  base::ObserverList<Observer>::Unchecked observers_;

  // The errors which we have received so far.
  ErrorMap errors_;

  // The default mask to use if an Extension does not have specific settings.
  int32_t default_mask_;

  // The profile with which the ErrorConsole is associated. Only collect errors
  // from extensions and RenderViews associated with this Profile (and it's
  // incognito fellow).
  Profile* profile_;

  // The ExtensionPrefs with which the ErrorConsole is associated. This weak
  // pointer is safe because ErrorConsole is owned by ExtensionSystem, which
  // is dependent on ExtensionPrefs.
  ExtensionPrefs* prefs_;

  ScopedObserver<Profile, ProfileObserver> profile_observer_{this};
  PrefChangeRegistrar pref_registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ErrorConsole);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_
