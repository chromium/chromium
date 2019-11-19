// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NOTE_TAKING_CLIENT_H_
#define ASH_PUBLIC_CPP_NOTE_TAKING_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

namespace ash {

// Interface for ash to notify the client (e.g. Chrome) about the new note
// creation.
class ASH_PUBLIC_EXPORT NoteTakingClient {
 public:
  static NoteTakingClient* GetInstance();

  // Returns true when it can create notes.
  virtual bool CanCreateNote() = 0;

  // Called when the controller needs to create a new note.
  virtual void CreateNote() = 0;

 protected:
  NoteTakingClient();
  virtual ~NoteTakingClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(NoteTakingClient);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NOTE_TAKING_CLIENT_H_
