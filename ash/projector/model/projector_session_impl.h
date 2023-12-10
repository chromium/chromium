// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_MODEL_PROJECTOR_SESSION_IMPL_H_
#define ASH_PROJECTOR_MODEL_PROJECTOR_SESSION_IMPL_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_EXPORT ProjectorSessionImpl : public ProjectorSession {
 public:
  ProjectorSessionImpl();
  ProjectorSessionImpl(const ProjectorSessionImpl&) = delete;
  ProjectorSessionImpl& operator=(const ProjectorSessionImpl&) = delete;
  ~ProjectorSessionImpl() override;

  const base::SafeBaseName& storage_dir() const { return storage_dir_; }
  void set_screencast_container_path(
      const base::FilePath& screencast_container_path) {
    screencast_container_path_ = screencast_container_path;
  }
  const std::optional<base::FilePath>& screencast_container_path() const {
    return screencast_container_path_;
  }
  const std::string& screencast_name() const { return screencast_name_; }

  // Start a projector session. `storage_dir` is the container directory name
  // for screencasts and will be used to create the storage path.
  void Start(const base::SafeBaseName& storage_dir);
  void Stop();

  void AddObserver(ProjectorSessionObserver* observer) override;
  void RemoveObserver(ProjectorSessionObserver* observer) override;

  // Get the screencast file path without file extension. This will be used
  // to construct media and metadata file path.
  base::FilePath GetScreencastFilePathNoExtension() const;

 private:
  void NotifySessionActiveStateChanged(bool active);

  base::SafeBaseName storage_dir_;

  // The file path of the screencast container. Only contains value after
  // recording is started and the container directory is created. Value will be
  // reset when Projector session is stopped.
  std::optional<base::FilePath> screencast_container_path_;
  // The name of screencast should be consistent with container folder, metadata
  // file and media file.
  std::string screencast_name_;

  base::ObserverList<ProjectorSessionObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_MODEL_PROJECTOR_SESSION_IMPL_H_
