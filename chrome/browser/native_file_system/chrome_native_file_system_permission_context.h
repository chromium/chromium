// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "base/sequence_checker.h"
#include "chrome/browser/permissions/permission_util.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class HostContentSettingsMap;

namespace content {
class BrowserContext;
}  // namespace content

// Chrome implementation of NativeFileSystemPermissionContext. Currently
// implements a single per-origin write permission state.
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

  class WritePermissionGrantImpl
      : public content::NativeFileSystemPermissionGrant {
   public:
    // In the current implementation permission grants are scoped to the frame
    // they are requested in. Within a frame we only want to have one grant per
    // path. The Key struct contains these fields. Keys are comparable so they
    // can be used with sorted containers like std::map and std::set.
    // TODO(https://crbug.com/984769): Eliminate process_id and frame_id and
    // replace usage of this struct with just a file path when grants stop being
    // scoped to a frame.
    struct Key {
      base::FilePath path;
      int process_id = 0;
      int frame_id = 0;

      bool operator==(const Key& rhs) const;
      bool operator<(const Key& rhs) const;
    };

    WritePermissionGrantImpl(
        base::WeakPtr<ChromeNativeFileSystemPermissionContext> context,
        const url::Origin& origin,
        const Key& key,
        bool is_directory);

    // content::NativeFileSystemPermissionGrant implementation:
    PermissionStatus GetStatus() override;
    void RequestPermission(
        int process_id,
        int frame_id,
        base::OnceCallback<void(PermissionRequestOutcome)> callback) override;

    const url::Origin& origin() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return origin_;
    }

    bool is_directory() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return is_directory_;
    }

    const base::FilePath& path() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return key_.path;
    }

    const Key& key() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return key_;
    }

    // Returns true if the |content_setting_guard_type_| has not been blocked.
    bool CanRequestPermission();

    void SetStatus(PermissionStatus status);

   protected:
    ~WritePermissionGrantImpl() override;

   private:
    void OnPermissionRequestComplete(
        base::OnceCallback<void(PermissionRequestOutcome)> callback,
        PermissionRequestOutcome outcome,
        PermissionAction result);

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtr<ChromeNativeFileSystemPermissionContext> const context_;
    const url::Origin origin_;
    const Key key_;
    const bool is_directory_;

    // This member should only be updated via SetStatus(), to make sure
    // observers are properly notified about any change in status.
    PermissionStatus status_ = PermissionStatus::ASK;

    DISALLOW_COPY_AND_ASSIGN(WritePermissionGrantImpl);
  };

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         bool is_directory,
                         int process_id,
                         int frame_id) override;

  bool CanRequestWritePermission(const url::Origin& origin) override;
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory,
                          int process_id,
                          int frame_id,
                          UserAction user_action) override;
  void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      bool is_directory,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) override;
  void ConfirmDirectoryReadAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionStatus)> callback) override;

  void PerformAfterWriteChecks(
      std::unique_ptr<content::NativeFileSystemWriteItem> item,
      int process_id,
      int frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;

  // Returns a snapshot of the currently granted permissions.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id from this
  // method when grants stop being scoped to a frame.
  struct Grants {
    Grants();
    ~Grants();
    Grants(Grants&&);
    Grants& operator=(Grants&&);

    std::vector<base::FilePath> file_write_grants;
    std::vector<base::FilePath> directory_write_grants;
  };
  Grants GetPermissionGrants(const url::Origin& origin,
                             int process_id,
                             int frame_id);

  // Revokes directory read access for the given origin in the given tab.
  void RevokeDirectoryReadGrants(const url::Origin& origin,
                                 int process_id,
                                 int frame_id);
  // Revokes write access for the given origin in the given tab.
  void RevokeWriteGrants(const url::Origin& origin,
                         int process_id,
                         int frame_id);

  // Revokes write access and directory read access for the given origin in the
  // given tab.
  void RevokeGrantsForOriginAndTab(const url::Origin& origin,
                                   int process_id,
                                   int frame_id);

  HostContentSettingsMap* content_settings() { return content_settings_.get(); }

 private:
  void PermissionGrantDestroyed(WritePermissionGrantImpl* grant);

  void DidConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      bool is_directory,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback,
      bool should_block);

  SEQUENCE_CHECKER(sequence_checker_);

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  scoped_refptr<HostContentSettingsMap> content_settings_;

  base::WeakPtrFactory<ChromeNativeFileSystemPermissionContext> weak_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(ChromeNativeFileSystemPermissionContext);
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
