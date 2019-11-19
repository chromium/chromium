// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pepper_flash_component_installer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_installer_errors.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/ppapi_utils.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/pepper_plugin_info.h"
#include "crypto/sha2.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#elif defined(OS_LINUX)
#include "chrome/common/component_flash_hint_file_linux.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;
using content::PluginService;

namespace component_updater {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if defined(OS_CHROMEOS)
// CRX hash for Chrome OS. The extension id is:
// ckjlcfmdbdglblbjglepgnoekdnkoklc.
const uint8_t kFlashSha2Hash[] = {
    0x2a, 0x9b, 0x25, 0xc3, 0x13, 0x6b, 0x1b, 0x19, 0x6b, 0x4f, 0x6d,
    0xe4, 0xa3, 0xda, 0xea, 0xb2, 0x67, 0xeb, 0xf0, 0xbb, 0x1f, 0x48,
    0xa2, 0x73, 0xea, 0x47, 0x11, 0xc8, 0x2b, 0xd9, 0x03, 0xb5};
#else
// CRX hash. The extension id is: mimojjlkmoijpicakmndhoigimigcmbb.
const uint8_t kFlashSha2Hash[] = {
    0xc8, 0xce, 0x99, 0xba, 0xce, 0x89, 0xf8, 0x20, 0xac, 0xd3, 0x7e,
    0x86, 0x8c, 0x86, 0x2c, 0x11, 0xb9, 0x40, 0xc5, 0x55, 0xaf, 0x08,
    0x63, 0x70, 0x54, 0xf9, 0x56, 0xd3, 0xe7, 0x88, 0xba, 0x8c};
#endif  // defined(OS_CHROMEOS)
static_assert(base::size(kFlashSha2Hash) == crypto::kSHA256Length,
              "Wrong hash length");

#if defined(OS_CHROMEOS)
void LogRegistrationResult(base::Optional<bool> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!result.has_value()) {
    LOG(ERROR) << "Call to imageloader service failed.";
    return;
  }
  if (!result.value()) {
    LOG(ERROR) << "Component flash registration failed";
    return;
  }
  SystemTrayClient* tray = SystemTrayClient::Get();
  if (tray) {
    tray->SetFlashUpdateAvailable();
  }
}

void ImageLoaderRegistration(const std::string& version,
                             const base::FilePath& install_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::ImageLoaderClient* loader =
      chromeos::DBusThreadManager::Get()->GetImageLoaderClient();

  if (loader) {
    loader->RegisterComponent("PepperFlashPlayer", version, install_dir.value(),
                              base::BindOnce(&LogRegistrationResult));
  } else {
    LOG(ERROR) << "Failed to get ImageLoaderClient object.";
  }
}

// Determine whether or not to skip registering flash component updates.
bool SkipFlashRegistration(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(features::kCrosCompUpdates))
    return true;

  // If the version of Chrome is pinned on the device (probably via enterprise
  // policy), do not component update Flash player.
  chromeos::CrosSettingsProvider::TrustedStatus status =
      chromeos::CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&RegisterPepperFlashComponent, cus));

  // Only if the settings are trusted, read the update settings and allow them
  // to disable Flash component updates. If the settings are untrusted, then we
  // fail-safe and allow the security updates.
  std::string version_prefix;
  bool update_disabled = false;
  switch (status) {
    case chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Return and allow flash registration to occur once the settings are
      // trusted.
      return true;
    case chromeos::CrosSettingsProvider::TRUSTED:
      chromeos::CrosSettings::Get()->GetBoolean(chromeos::kUpdateDisabled,
                                                &update_disabled);
      chromeos::CrosSettings::Get()->GetString(chromeos::kTargetVersionPrefix,
                                               &version_prefix);

      return update_disabled || !version_prefix.empty();
    case chromeos::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      return false;
  }

  // Default to not skipping component flash registration since updates are
  // security critical.
  return false;
}
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if !defined(OS_LINUX) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
bool MakePepperFlashPluginInfo(const base::FilePath& flash_path,
                               const base::Version& flash_version,
                               bool out_of_process,
                               content::PepperPluginInfo* plugin_info) {
  if (!flash_version.IsValid())
    return false;
  const std::vector<uint32_t> ver_nums = flash_version.components();
  if (ver_nums.size() < 3)
    return false;

  plugin_info->is_internal = false;
  plugin_info->is_out_of_process = out_of_process;
  plugin_info->path = flash_path;
  plugin_info->name = content::kFlashPluginName;
  plugin_info->permissions = kPepperFlashPermissions;

  // The description is like "Shockwave Flash 10.2 r154".
  plugin_info->description = base::StringPrintf("%s %d.%d r%d",
                                                content::kFlashPluginName,
                                                ver_nums[0],
                                                ver_nums[1],
                                                ver_nums[2]);

  plugin_info->version = flash_version.GetString();

  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginName);
  plugin_info->mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginName);
  plugin_info->mime_types.push_back(spl_mime_type);
  return true;
}

// |path| is the path to the latest Chrome-managed Flash installation (bundled
// or component updated).
// |version| is the version of that Flash implementation.
void RegisterPepperFlashWithChrome(const base::FilePath& path,
                                   const base::Version& version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::PepperPluginInfo plugin_info;
  if (!MakePepperFlashPluginInfo(path, version, true, &plugin_info))
    return;
  content::WebPluginInfo web_plugin = plugin_info.ToWebPluginInfo();

  base::FilePath system_flash_path;
  base::PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                         &system_flash_path);

  std::vector<content::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);
  for (const auto& plugin : plugins) {
    if (!plugin.is_pepper_plugin() || plugin.name != web_plugin.name)
      continue;

    if (plugin.path.value() == ChromeContentClient::kNotPresent) {
      // This is the Flash placeholder; replace it regardless of version or
      // other considerations.
      PluginService::GetInstance()->UnregisterInternalPlugin(plugin.path);
      break;
    }

    base::Version registered_version(base::UTF16ToUTF8(plugin.version));

    // If lower or equal version, never register.
    if (registered_version.IsValid() &&
        version.CompareTo(registered_version) <= 0) {
      return;
    }

    // If the version is newer, remove the old one first.
    PluginService::GetInstance()->UnregisterInternalPlugin(plugin.path);
    break;
  }

  PluginService::GetInstance()->RegisterInternalPlugin(web_plugin, true);
  PluginService::GetInstance()->RefreshPlugins();
}

void UpdatePathService(const base::FilePath& path) {
  base::PathService::Override(chrome::DIR_PEPPER_FLASH_PLUGIN, path);
}
#endif  // !defined(OS_LINUX) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
class FlashComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  FlashComponentInstallerPolicy();
  ~FlashComponentInstallerPolicy() override {}

 private:
  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  DISALLOW_COPY_AND_ASSIGN(FlashComponentInstallerPolicy);
};

FlashComponentInstallerPolicy::FlashComponentInstallerPolicy() {}

bool FlashComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool FlashComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
FlashComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  std::string version;
  if (!manifest.GetString("version", &version)) {
    return update_client::ToInstallerResult(
        FlashError::MISSING_VERSION_IN_MANIFEST);
  }

#if defined(OS_CHROMEOS)
  base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
      ->PostTask(FROM_HERE, base::BindOnce(&ImageLoaderRegistration, version,
                                           install_dir));
#elif defined(OS_LINUX)
  const base::FilePath flash_path =
      install_dir.Append(chrome::kPepperFlashPluginFilename);
  // Populate the component updated flash hint file so that the zygote can
  // locate and preload the latest version of flash.
  if (!component_flash_hint_file::RecordFlashUpdate(flash_path, flash_path,
                                                    version)) {
    return update_client::ToInstallerResult(FlashError::HINT_FILE_RECORD_ERROR);
  }
#endif  // defined(OS_LINUX)
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void FlashComponentInstallerPolicy::OnCustomUninstall() {}

void FlashComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
#if !defined(OS_LINUX)
  // Installation is done. Now tell the rest of chrome. Both the path service
  // and to the plugin service. On Linux, a restart is required to use the new
  // Flash version, so we do not do this.
  RegisterPepperFlashWithChrome(path.Append(chrome::kPepperFlashPluginFilename),
                                version);
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&UpdatePathService, path));
#endif  // !defined(OS_LINUX)
}

bool FlashComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  base::Version unused;
  return CheckPepperFlashManifest(manifest, &unused);
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\PepperFlash\.
base::FilePath FlashComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("PepperFlash"));
}

void FlashComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kFlashSha2Hash, kFlashSha2Hash + base::size(kFlashSha2Hash));
}

std::string FlashComponentInstallerPolicy::GetName() const {
  return "Adobe Flash Player";
}

update_client::InstallerAttributes
FlashComponentInstallerPolicy::GetInstallerAttributes() const {
  // For Chrome OS, send the built-in flash player version to the server,
  // otherwise it will serve component updates of outdated flash players.
  update_client::InstallerAttributes attrs;
#if defined(OS_CHROMEOS)
  const std::string flash_version =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPpapiFlashVersion);
  attrs["built_in_version"] = flash_version;
#endif  // #defined(OS_CHROMEOS)
  return attrs;
}

std::vector<std::string> FlashComponentInstallerPolicy::GetMimeTypes() const {
  std::vector<std::string> mime_types;
  mime_types.push_back("application/x-shockwave-flash");
  mime_types.push_back("application/futuresplash");
  return mime_types;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

void RegisterPepperFlashComponent(ComponentUpdateService* cus) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Component updated flash supersedes bundled flash therefore if that one
  // is disabled then this one should never install.
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBundledPpapiFlash))
    return;

#if defined(OS_CHROMEOS)
  if (SkipFlashRegistration(cus))
    return;
#endif  // defined(OS_CHROMEOS)

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<FlashComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace component_updater
