// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_TEST_WATCHDOG_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_TEST_WATCHDOG_H_

#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::cfm {

class TestWatchDog : public mojom::DataWatchDog {
 public:
  TestWatchDog(mojo::PendingReceiver<mojom::DataWatchDog> receiver,
               mojom::DataFilterPtr filter);
  TestWatchDog(const TestWatchDog&) = delete;
  TestWatchDog& operator=(const TestWatchDog&) = delete;
  ~TestWatchDog() override;

  const mojom::DataFilterPtr GetFilter();

 protected:
  // mojom::DataWatchDog implementation
  void OnNotify(const std::string& data) override;

 private:
  mojo::Receiver<mojom::DataWatchDog> receiver_;
  const mojom::DataFilterPtr filter_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_TEST_WATCHDOG_H_
