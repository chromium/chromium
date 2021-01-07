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
class NoDestructor;

class SequencedTaskRunner;
}  // namespace base

namespace ash {

enum class State { INITIALIZING, SUCCESS, FAILED };

enum class ECLidAngleDriverStatus { UNKNOWN, SUPPORTED, NOT_SUPPORTED };

class AccelerometerProviderInterface;

// AccelerometerReader should only be used on the UI thread.
// It notifies observers if EC Lid Angle Driver is supported, and provides
// accelerometers' (lid and base) samples.
// The current usages of accelerometers' samples are for calculating the angle
// between the lid and the base, which can be substituted by EC Lid Angle
// Driver, if it exists, and the auto rotation, which only needs
// lid-accelerometer's data.
// Therefore, if EC Lid Angle Driver is present, base-accelerometer's samples
// may be ignored and not sent to the observers.
class ASH_EXPORT AccelerometerReader {
 public:
  // An interface to receive data from the AccelerometerReader.
  class Observer {
   public:
    virtual void OnAccelerometerUpdated(const AccelerometerUpdate& update) = 0;

   protected:
    virtual ~Observer() {}
  };

  static AccelerometerReader* GetInstance();

  void Initialize();

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
  friend class base::NoDestructor<AccelerometerReader>;

  // Worker that will run on the base::SequencedTaskRunner provided to
  // Initialize. It will determine accelerometer configuration, read the data,
  // and notify observers.
  scoped_refptr<AccelerometerProviderInterface> accelerometer_provider_;
};

class AccelerometerProviderInterface
    : public base::RefCountedThreadSafe<AccelerometerProviderInterface> {
 public:
  // Prepare and start async initialization.
  virtual void PrepareAndInitialize() = 0;

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

  // The current initialization state of reader.
  State initialization_state_ = State::INITIALIZING;

  // State of ChromeOS EC lid angle driver, if SUPPORTED, it means EC can handle
  // lid angle calculation.
  ECLidAngleDriverStatus ec_lid_angle_driver_status_ =
      ECLidAngleDriverStatus::UNKNOWN;

 private:
  friend class base::RefCountedThreadSafe<AccelerometerProviderInterface>;
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_READER_H_
