// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_COMMAND_SOURCE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_COMMAND_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/local_data_source.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cfm {

// This class tracks the output of a particular command.
class CommandSource : public LocalDataSource {
 public:
  CommandSource(const std::string& command, base::TimeDelta poll_rate);
  CommandSource(const CommandSource&) = delete;
  CommandSource& operator=(const CommandSource&) = delete;
  ~CommandSource() override;

  // LocalDataSource:
  const std::string& GetDisplayName() override;
  std::vector<std::string> GetNextData() override;

 private:
  std::string command_;
  std::vector<std::string> command_split_;

  // Must be the last class member.
  base::WeakPtrFactory<CommandSource> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_COMMAND_SOURCE_H_
