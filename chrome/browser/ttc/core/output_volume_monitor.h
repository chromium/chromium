// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_OUTPUT_VOLUME_MONITOR_H_
#define CHROME_BROWSER_TTC_CORE_OUTPUT_VOLUME_MONITOR_H_

#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/functional/callback.h"

namespace ttc {

// The volume of the system's default audio output, which TTC plays the model's
// replies on.
struct OutputVolumeState {
  bool muted = false;
  // From 0 (silent) to 1 (loudest). Null if the device has no volume control.
  std::optional<float> level;

  friend bool operator==(const OutputVolumeState&,
                         const OutputVolumeState&) = default;
};

// Reports the volume of the system's default audio output. It reads the OS
// mixer from the browser process: the audio service runs sandboxed on most
// platforms and has no API for it.
class OutputVolumeMonitor {
 public:
  using StateChangedCallback =
      base::RepeatingCallback<void(const OutputVolumeState&)>;
  using Factory = base::RepeatingCallback<std::unique_ptr<OutputVolumeMonitor>(
      StateChangedCallback)>;

  // Makes `Create()` use `factory` while this object lives.
  class ScopedFactoryForTesting {
   public:
    explicit ScopedFactoryForTesting(Factory factory);
    ScopedFactoryForTesting(const ScopedFactoryForTesting&) = delete;
    ScopedFactoryForTesting& operator=(const ScopedFactoryForTesting&) = delete;
    ~ScopedFactoryForTesting();

   private:
    base::AutoReset<Factory> reset_;
  };

  // Returns null where TTC can't read the output volume. `callback` runs on
  // the calling sequence each time the state changes, never synchronously
  // from this call.
  static std::unique_ptr<OutputVolumeMonitor> Create(
      StateChangedCallback callback);

  virtual ~OutputVolumeMonitor() = default;

  // The current state, or null if it isn't known yet.
  virtual std::optional<OutputVolumeState> GetState() const = 0;

 private:
  // Defined in output_volume_monitor_<platform>.cc, or in
  // output_volume_monitor.cc as a null fallback on platforms without an
  // implementation.
  static std::unique_ptr<OutputVolumeMonitor> CreateForPlatform(
      StateChangedCallback callback);
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_OUTPUT_VOLUME_MONITOR_H_
