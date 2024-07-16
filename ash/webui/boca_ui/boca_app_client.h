// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_APP_CLIENT_H_
#define ASH_WEBUI_BOCA_UI_BOCA_APP_CLIENT_H_

#include "ash/webui/boca_ui/proto/bundle.pb.h"
#include "ash/webui/boca_ui/proto/session.pb.h"
#include "base/observer_list_types.h"

namespace ash {

// Defines the interface for sub features to access hub Events
class BocaAppClient {
 public:
  // Interface for observing events.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies when session started. Pure virtual function, must be handled by
    // observer.
    virtual void OnSessionStarted(const std::string& session_id) = 0;

    // Notifies when session ended. Pure virtual function, must be handled by
    // observer.
    virtual void OnSessionEnded(const std::string& session_id) = 0;

    // Notifies when bundle updated. In the event of session started with a
    // bundle configured, both events will be fired.
    virtual void OnBundleUpdated(const boca::Bundle& bundle);

    // Notifies when caption producer's config updated.
    virtual void OnProducerCaptionConfigUpdated(
        const boca::CaptionsConfig& config);

    // Notifies when caption consumer's config updated.
    virtual void OnConsumerCaptionConfigUpdated(
        const boca::CaptionsConfig& config);
  };

  BocaAppClient(const BocaAppClient&) = delete;
  BocaAppClient& operator=(const BocaAppClient&) = delete;

  static BocaAppClient* Get();

  // Returns `true` if contains producer attribute.
  virtual bool IsProducer() = 0;

  // Returns `true` if contains consumer attribute.
  virtual bool IsConsumer() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  BocaAppClient();
  virtual ~BocaAppClient();
};

}  // namespace ash

#endif  // ASH_WEBUI_BOCA_UI_BOCA_APP_CLIENT_H_
