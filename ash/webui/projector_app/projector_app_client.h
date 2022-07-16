// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_

#include <set>

#include "base/files/file_path.h"
#include "base/observer_list_types.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace base {
class Value;
}  // namespace base

namespace ash {

// TODO(b/201468756): pendings screencasts are sorted by created time. Add
// `created_time` field to PendingScreencast. Screencasts might fail to
// upload. Add `failed_to_upload` field to PendingScreencast. Implement upload
// progress and add a custom comparator.
struct PendingScreencast {
  base::Value ToValue() const;
  bool operator==(const PendingScreencast& rhs) const;
  bool operator<(const PendingScreencast& rhs) const;

  // The container path of the screencast. It's a relative path of drive, looks
  // like "/root/projector_data/abc".
  base::FilePath container_dir;
  // The display name of screencast. If `container_dir` is
  // "/root/projector_data/abc", the `name` is "abc".
  std::string name;
};

// Defines interface to access Browser side functionalities for the
// ProjectorApp.
class ProjectorAppClient {
 public:
  // Interface for observing events on the ProjectorAppClient.
  class Observer : public base::CheckedObserver {
   public:
    // Used to notify the Projector SWA app on whether it can start a new
    // screencast session.
    virtual void OnNewScreencastPreconditionChanged(bool can_start) = 0;

    // Observes the pending screencast state change events.
    virtual void OnScreencastsPendingStatusChanged(
        const std::set<PendingScreencast>& pending_screencast) = 0;
  };

  ProjectorAppClient(const ProjectorAppClient&) = delete;
  ProjectorAppClient& operator=(const ProjectorAppClient&) = delete;

  static ProjectorAppClient* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the IdentityManager for the primary user profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the URLLoaderFactory for the primary user profile.
  virtual network::mojom::URLLoaderFactory* GetUrlLoaderFactory() = 0;

  // Used to notify the Projector SWA app on whether it can start a new
  // screencast session.
  virtual void OnNewScreencastPreconditionChanged(bool can_start) = 0;

  // Returns pending screencast uploaded by primary user.
  virtual const std::set<PendingScreencast>& GetPendingScreencasts() const = 0;

 protected:
  ProjectorAppClient();
  virtual ~ProjectorAppClient();
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
