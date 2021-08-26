// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_MODEL_PROJECTOR_SESSION_IMPL_H_
#define ASH_PROJECTOR_MODEL_PROJECTOR_SESSION_IMPL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_EXPORT ProjectorSessionImpl : public ProjectorSession {
 public:
  ProjectorSessionImpl();
  ProjectorSessionImpl(const ProjectorSessionImpl&) = delete;
  ProjectorSessionImpl& operator=(const ProjectorSessionImpl&) = delete;
  ~ProjectorSessionImpl() override;

  const std::string& storage_dir() const { return storage_dir_; }

  // Start a projector session. `storage_dir` is the container directory name
  // for screencasts and will be used to create the storage path.
  void Start(const std::string& storage_dir);
  void Stop();

  void AddObserver(ProjectorSessionObserver* observer) override;
  void RemoveObserver(ProjectorSessionObserver* observer) override;

 private:
  void NotifySessionActiveStateChanged(bool active);

  std::string storage_dir_;

  base::ObserverList<ProjectorSessionObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_MODEL_PROJECTOR_SESSION_IMPL_H_
