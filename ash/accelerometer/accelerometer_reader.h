// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_READER_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_READER_H_

#include "ash/accelerometer/accelerometer_types.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;

class SequencedTaskRunner;
}  // namespace base

namespace ash {

class AccelerometerFileReader;

// Reads an accelerometer device and reports data back to an
// AccelerometerDelegate.
class ASH_EXPORT AccelerometerReader {
 public:

  // An interface to receive data from the AccelerometerReader.
  class Observer {
   public:
    virtual void OnAccelerometerUpdated(
        scoped_refptr<const AccelerometerUpdate> update) = 0;

   protected:
    virtual ~Observer() {}
  };

  static AccelerometerReader* GetInstance();

  void Initialize(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Accelerometer file reader starts/stops listening to tablet mode controller.
  void StartListenToTabletModeController();
  void StopListenToTabletModeController();

 protected:
  AccelerometerReader();
  virtual ~AccelerometerReader();

 private:
  friend struct base::DefaultSingletonTraits<AccelerometerReader>;

  // Worker that will run on the base::SequencedTaskRunner provided to
  // Initialize. It will determine accelerometer configuration, read the data,
  // and notify observers.
  scoped_refptr<AccelerometerFileReader> accelerometer_file_reader_;

  DISALLOW_COPY_AND_ASSIGN(AccelerometerReader);
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_READER_H_
