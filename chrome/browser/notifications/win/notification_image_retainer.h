// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_IMAGE_RETAINER_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_IMAGE_RETAINER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace gfx {
class Image;
}  // namespace gfx

// The purpose of this class is to take data from memory, store it to disk as
// temp files and keep them alive long enough to hand over to external entities,
// such as the Action Center on Windows. The Action Center will read the files
// at some point in the future, which is why we can't do:
//   [write file] -> [show notification] -> [delete file].
//
// Also, on Windows, temp file deletion is not guaranteed and, since the images
// can potentially be large, this presents a problem because Chrome might then
// be leaving chunks of dead bits lying around on users' computers during
// unclean shutdowns.
class NotificationImageRetainer {
 public:
  NotificationImageRetainer(
      scoped_refptr<base::SequencedTaskRunner> deletion_task_runner,
      const base::TickClock* tick_clock);

  NotificationImageRetainer();
  virtual ~NotificationImageRetainer();

  // Deletes all the remaining files in image_dir_ due to previous unclean
  // shutdowns.
  virtual void CleanupFilesFromPrevSessions();

  // Stores an |image| on disk in a temporary (short-lived) file. Returns the
  // path to the file created, which will be valid for a few seconds only. It
  // will be deleted either after a short timeout or after a restart of Chrome.
  // The function returns an empty FilePath if file creation fails.
  virtual base::FilePath RegisterTemporaryImage(const gfx::Image& image);

  // Returns a closure that, when run, performs cleanup operations. This closure
  // must be run on the notification sequence.
  base::OnceClosure GetCleanupTask();

  const base::FilePath& image_dir() { return image_dir_; }

 private:
  using NameAndTime = std::pair<base::FilePath, base::TimeTicks>;
  using NamesAndTimes = std::vector<NameAndTime>;

  // Deletes expired (older than a pre-defined threshold) files.
  void DeleteExpiredFiles();

  // A collection of names (note: not full paths) to registered image files
  // in image_dir_, each of which must stay valid for a short time while the
  // Notification Center processes them. Each file has a corresponding
  // registration timestamp. Files in this collection that have outlived the
  // required minimum lifespan are scheduled for deletion periodically by
  // |deletion_timer_|. The items in this collection are sorted by increasing
  // registration time.
  NamesAndTimes registered_images_;

  // The task runner used to handle file deletion.
  scoped_refptr<base::SequencedTaskRunner> deletion_task_runner_;

  // The path to where to store the temporary files.
  const base::FilePath image_dir_;

  // Not owned.
  const base::TickClock* const tick_clock_;

  // A timer used to handle deleting files in batch.
  base::RepeatingTimer deletion_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // For callbacks may run after destruction.
  base::WeakPtrFactory<NotificationImageRetainer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NotificationImageRetainer);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_IMAGE_RETAINER_H_
