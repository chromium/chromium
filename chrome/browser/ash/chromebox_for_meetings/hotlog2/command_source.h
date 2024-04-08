// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_COMMAND_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_COMMAND_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cfm {

// This class tracks the output of a particular command.
class CommandSource : public mojom::DataSource {
 public:
  CommandSource(const std::string& command, base::TimeDelta poll_rate);
  CommandSource(const CommandSource&) = delete;
  CommandSource& operator=(const CommandSource&) = delete;
  ~CommandSource() override;

 protected:
  // mojom::DataSource implementation
  void Fetch(FetchCallback callback) override;
  void AddWatchDog(mojom::DataFilterPtr filter,
                   mojo::PendingRemote<mojom::DataWatchDog> watch_dog,
                   AddWatchDogCallback callback) override;
  void Flush() override;

 private:
  void StartPollTimer();
  void StoreCommandOutputIfDifferent();

  base::RepeatingTimer poll_timer_;

  std::string command_;
  std::vector<std::string> command_split_;
  base::TimeDelta poll_rate_;

  // Contains the most recent command output. Only updated if
  // the output is different from the last last_output_ value.
  std::string last_output_;

  // Contains a chain of the most recent (unique) command outputs.
  // Will be moved into pending_upload_buffer_ below upon a call
  // to Fetch().
  std::vector<std::string> command_buffer_;

  // Contains a chain of command outputs that are queued for upload.
  // Will be cleared upon a call to Flush();
  std::vector<std::string> pending_upload_buffer_;

  // Must be the last class member.
  base::WeakPtrFactory<CommandSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_COMMAND_SOURCE_H_
