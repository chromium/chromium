// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_COMMAND_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_COMMAND_SOURCE_H_

#include <base/memory/weak_ptr.h>

#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"

namespace ash::cfm {

// This class tracks the output of a particular command.
class CommandSource : public mojom::DataSource {
 public:
  CommandSource(std::string command, bool should_be_uploaded);
  CommandSource(const CommandSource&) = delete;
  CommandSource& operator=(const CommandSource&) = delete;
  ~CommandSource() override;

 protected:
  // mojom::DataSource implementation
  void GetSourceName(GetSourceNameCallback callback) override;
  void Fetch(FetchCallback callback) override;
  void AddWatchDog(mojo::PendingRemote<mojom::DataWatchDog> watch_dog) override;
  void ShouldBeUploaded(ShouldBeUploadedCallback callback) override;

 private:
  std::string command_;
  std::vector<std::string> command_split_;
  bool should_be_uploaded_;

  // Must be the last class member.
  base::WeakPtrFactory<CommandSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_COMMAND_SOURCE_H_
