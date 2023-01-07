// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_NETWORK_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_NETWORK_SPEECH_RECOGNIZER_H_

#include <string>

#include "chrome/browser/speech/speech_recognizer.h"

namespace network {
class PendingSharedURLLoaderFactory;
}

class SpeechRecognizerDelegate;

// NetworkSpeechRecognizer is a wrapper around the speech recognition engine
// that simplifies its use from the UI thread. This class handles all
// setup/shutdown, collection of results, error cases, and threading.
class NetworkSpeechRecognizer : public SpeechRecognizer {
 public:
  NetworkSpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_shared_url_loader_factory,
      const std::string& accept_language,
      const std::string& locale);
  ~NetworkSpeechRecognizer() override;
  NetworkSpeechRecognizer(const NetworkSpeechRecognizer&) = delete;
  NetworkSpeechRecognizer& operator=(const NetworkSpeechRecognizer&) = delete;

  // Must be called on the UI thread.
  void Start() override;
  void Stop() override;

 private:
  class EventListener;
  scoped_refptr<EventListener> speech_event_listener_;
};

#endif  // CHROME_BROWSER_SPEECH_NETWORK_SPEECH_RECOGNIZER_H_
