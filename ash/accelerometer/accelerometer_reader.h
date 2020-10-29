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

enum class ECLidAngleDriverStatus { UNKNOWN, SUPPORTED, NOT_SUPPORTED };

class AccelerometerProviderInterface;

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

  // Controls the availability of emitting acccelerometer reader events to
  // its observers. This shouldn't be called normally, but Tast tests should
  // be able to control the accelerometer feature.
  void SetEnabled(bool enabled);

  // Return the state of the driver being supported or not.
  ECLidAngleDriverStatus GetECLidAngleDriverStatus() const;

  void SetECLidAngleDriverStatusForTesting(
      ECLidAngleDriverStatus ec_lid_angle_driver_status);

 protected:
  AccelerometerReader();
  AccelerometerReader(const AccelerometerReader&) = delete;
  AccelerometerReader& operator=(const AccelerometerReader&) = delete;
  virtual ~AccelerometerReader();

 private:
  friend struct base::DefaultSingletonTraits<AccelerometerReader>;

  // Worker that will run on the base::SequencedTaskRunner provided to
  // Initialize. It will determine accelerometer configuration, read the data,
  // and notify observers.
  scoped_refptr<AccelerometerProviderInterface> accelerometer_provider_;
};

class AccelerometerProviderInterface
    : public base::RefCountedThreadSafe<AccelerometerProviderInterface> {
 public:
  // Prepare and start async initialization. SetSensorClient function
  // contains actual code for initialization.
  virtual void PrepareAndInitialize(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) = 0;

  // Add/Remove observers.
  virtual void AddObserver(AccelerometerReader::Observer* observer) = 0;
  virtual void RemoveObserver(AccelerometerReader::Observer* observer) = 0;

  // Start/Stop listening to tablet mode controller.
  virtual void StartListenToTabletModeController() = 0;
  virtual void StopListenToTabletModeController() = 0;

  // Set emitting events (samples) to observers or not.
  virtual void SetEmitEvents(bool emit_events) = 0;

  // Return the state of the driver being supported or not.
  ECLidAngleDriverStatus GetECLidAngleDriverStatus() const;

  void SetECLidAngleDriverStatusForTesting(
      ECLidAngleDriverStatus ec_lid_angle_driver_status);

 protected:
  virtual ~AccelerometerProviderInterface() = default;

  // State of ChromeOS EC lid angle driver, if SUPPORTED, it means EC can handle
  // lid angle calculation.
  ECLidAngleDriverStatus ec_lid_angle_driver_status_ =
      ECLidAngleDriverStatus::UNKNOWN;

 private:
  friend class base::RefCountedThreadSafe<AccelerometerProviderInterface>;
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_READER_H_
