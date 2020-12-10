// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "base/sequence_checker.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class HostContentSettingsMap;
enum ContentSetting;

namespace content {
class BrowserContext;
}  // namespace content

// Chrome implementation of NativeFileSystemPermissionContext. Currently chrome
// supports two different permissions models, each implemented in concrete
// subclasses of this class. This class itself implements the bits that are
// shared between the two models.
//
// All methods must be called on the UI thread.
//
// This class does not inherit from ChooserContextBase because the model this
// API uses doesn't really match what ChooserContextBase has to provide. The
// limited lifetime of native file system permission grants (scoped to the
// lifetime of the handles that reference the grants), and the possible
// interactions between grants for directories and grants for children of those
// directories as well as possible interactions between read and write grants
// make it harder to squeeze this into a shape that fits with
// ChooserContextBase.
class ChromeNativeFileSystemPermissionContext
    : public content::NativeFileSystemPermissionContext,
      public KeyedService {
 public:
  explicit ChromeNativeFileSystemPermissionContext(
      content::BrowserContext* context);
  ~ChromeNativeFileSystemPermissionContext() override;

  // content::NativeFileSystemPermissionContext:
  void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type,
      content::GlobalFrameRoutingId frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) override;
  void PerformAfterWriteChecks(
      std::unique_ptr<content::NativeFileSystemWriteItem> item,
      content::GlobalFrameRoutingId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;
  bool CanObtainReadPermission(const url::Origin& origin) override;
  bool CanObtainWritePermission(const url::Origin& origin) override;

  void SetLastPickedDirectory(const url::Origin& origin,
                              const base::FilePath& path,
                              const PathType type) override;
  PathInfo GetLastPickedDirectory(const url::Origin& origin) override;
  base::FilePath GetCommonDirectoryPath(
      blink::mojom::CommonDirectory directory) override;

  ContentSetting GetReadGuardContentSetting(const url::Origin& origin);
  ContentSetting GetWriteGuardContentSetting(const url::Origin& origin);

  // Returns a snapshot of the currently granted permissions.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id from this
  // method when grants stop being scoped to a frame.
  struct Grants {
    Grants();
    ~Grants();
    Grants(Grants&&);
    Grants& operator=(Grants&&);

    std::vector<base::FilePath> file_read_grants;
    std::vector<base::FilePath> file_write_grants;
    std::vector<base::FilePath> directory_read_grants;
    std::vector<base::FilePath> directory_write_grants;
  };
  virtual Grants GetPermissionGrants(const url::Origin& origin) = 0;

  // Revokes write access and directory read access for the given origin.
  virtual void RevokeGrants(const url::Origin& origin) = 0;

  virtual bool OriginHasReadAccess(const url::Origin& origin);
  virtual bool OriginHasWriteAccess(const url::Origin& origin);

  // Called by NativeFileSystemTabHelper when a top-level frame was navigated
  // away from |origin| to some other origin.
  virtual void NavigatedAwayFromOrigin(const url::Origin& origin) {}

  HostContentSettingsMap* content_settings() { return content_settings_.get(); }

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void DidConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      content::GlobalFrameRoutingId frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback,
      bool should_block);

  virtual base::WeakPtr<ChromeNativeFileSystemPermissionContext>
  GetWeakPtr() = 0;

  scoped_refptr<HostContentSettingsMap> content_settings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeFileSystemPermissionContext);
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
