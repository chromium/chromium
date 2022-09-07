// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_READER_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_READER_H_

#include "ash/accelerometer/accelerometer_types.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

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
    // Normally called only once, when
    // |AcceleromterProviderInterface::ec_lid_angle_driver_status_| is set to
    // either SUPPORTED or NOT_SUPPORTED, unless the lid angle driver is
    // late-present after timed out, which will be called twice with
    // |is_supported| being false and true respectively.
    // It's guaranteed to be called before |OnAccelerometerUpdated|.
    virtual void OnECLidAngleDriverStatusChanged(bool is_supported) = 0;
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

class ASH_EXPORT AccelerometerProviderInterface
    : public base::RefCountedThreadSafe<AccelerometerProviderInterface>,
      public TabletModeObserver {
 public:
  // Prepare and start async initialization.
  virtual void PrepareAndInitialize() = 0;
  // With ChromeOS EC lid angle driver present, it's triggered when the device
  // is physically used as a tablet (even thought its UI might be in clamshell
  // mode), cancelled otherwise.
  virtual void TriggerRead() = 0;
  virtual void CancelRead() = 0;

  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

  // Add/Remove observers.
  void AddObserver(AccelerometerReader::Observer* observer);
  void RemoveObserver(AccelerometerReader::Observer* observer);

  // Start/Stop listening to tablet mode controller.
  void StartListenToTabletModeController();
  void StopListenToTabletModeController();

  // Set emitting events (samples) to observers or not.
  void SetEmitEvents(bool emit_events);

  void SetECLidAngleDriverStatusForTesting(ECLidAngleDriverStatus status);

 protected:
  AccelerometerProviderInterface();
  ~AccelerometerProviderInterface() override;

  // Used in |OnTabletPhysicalStateChanged()|. As there might be
  // initialization steps, each implementation can override this function to
  // determine if this class is ready to process the state changed.
  // If returns true, |OnTabletPhysicalStateChanged()| will be skipped, and it's
  // the implementation's responsibility to call it again when the class is
  // ready. If returns false, |OnTabletPhysicalStateChanged()| will be processed
  // as usual.
  // Default to return false.
  virtual bool ShouldDelayOnTabletPhysicalStateChanged();

  void SetECLidAngleDriverStatus(ECLidAngleDriverStatus status);
  ECLidAngleDriverStatus GetECLidAngleDriverStatus() const;

  void NotifyAccelerometerUpdated(const AccelerometerUpdate& update);

  // Set in the constructor.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

 private:
  // State of ChromeOS EC lid angle driver, if SUPPORTED, it means EC can handle
  // lid angle calculation.
  ECLidAngleDriverStatus ec_lid_angle_driver_status_ =
      ECLidAngleDriverStatus::UNKNOWN;

  bool emit_events_ = true;

  // The observers to notify of accelerometer updates.
  // Bound to the UI thread.
  base::ObserverList<AccelerometerReader::Observer>::Unchecked observers_;

  friend class base::RefCountedThreadSafe<AccelerometerProviderInterface>;
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_READER_H_
