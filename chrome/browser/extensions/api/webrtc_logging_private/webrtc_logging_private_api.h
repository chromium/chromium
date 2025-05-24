// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_LOGGING_PRIVATE_WEBRTC_LOGGING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_LOGGING_PRIVATE_WEBRTC_LOGGING_PRIVATE_API_H_

#include <string>

#include "chrome/browser/media/webrtc/audio_debug_recordings_handler.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/common/extensions/api/webrtc_logging_private.h"
#include "extensions/browser/extension_function.h"
#include "media/media_buildflags.h"

namespace content {

class RenderProcessHost;

}

namespace extensions {

class WebrtcLoggingPrivateFunction : public ExtensionFunction {
 protected:
  ~WebrtcLoggingPrivateFunction() override = default;

  // Returns the RenderProcessHost associated with the given |request|
  // authorized by the |security_origin|. Returns null and sets |*error| to an
  // appropriate error if unauthorized or the RPH does not exist.
  content::RenderProcessHost* RphFromRequest(
      const api::webrtc_logging_private::RequestInfo& request,
      const std::string& security_origin,
      std::string* error);

  WebRtcLoggingController* LoggingControllerFromRequest(
      const api::webrtc_logging_private::RequestInfo& request,
      const std::string& security_origin,
      std::string* error);
};

class WebrtcLoggingPrivateFunctionWithGenericCallback
    : public WebrtcLoggingPrivateFunction {
 protected:
  ~WebrtcLoggingPrivateFunctionWithGenericCallback() override = default;

  // Finds the appropriate logging controller for performing the task and
  // prepares a generic callback object for when the task is completed.  If the
  // logging controller can't be found for the given request+origin, the
  // returned ptr will be null and |*error| will be set to an appropriate error
  // message.
  WebRtcLoggingController* PrepareTask(
      const api::webrtc_logging_private::RequestInfo& request,
      const std::string& security_origin,
      WebRtcLoggingController::GenericDoneCallback* callback,
      std::string* error);

  // Must be called on UI thread.
  void FireCallback(bool success, const std::string& error_message);
};

class WebrtcLoggingPrivateFunctionWithUploadCallback
    : public WebrtcLoggingPrivateFunction {
 protected:
  ~WebrtcLoggingPrivateFunctionWithUploadCallback() override = default;

  // Must be called on UI thread.
  void FireCallback(bool success, const std::string& report_id,
                    const std::string& error_message);
};

class WebrtcLoggingPrivateFunctionWithRecordingDoneCallback
    : public WebrtcLoggingPrivateFunction {
 protected:
  ~WebrtcLoggingPrivateFunctionWithRecordingDoneCallback() override = default;

  // Must be called on UI thread.
  void FireErrorCallback(const std::string& error_message);
  void FireCallback(const std::string& prefix_path,
                    bool did_stop,
                    bool did_manual_stop);
};

class WebrtcLoggingPrivateSetMetaDataFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.setMetaData",
                             WEBRTCLOGGINGPRIVATE_SETMETADATA)
  WebrtcLoggingPrivateSetMetaDataFunction() = default;

 private:
  ~WebrtcLoggingPrivateSetMetaDataFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStartFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.start",
                             WEBRTCLOGGINGPRIVATE_START)
  WebrtcLoggingPrivateStartFunction() = default;

 private:
  ~WebrtcLoggingPrivateStartFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateSetUploadOnRenderCloseFunction
    : public WebrtcLoggingPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.setUploadOnRenderClose",
                             WEBRTCLOGGINGPRIVATE_SETUPLOADONRENDERCLOSE)
  WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() = default;

 private:
  ~WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStopFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.stop",
                             WEBRTCLOGGINGPRIVATE_STOP)
  WebrtcLoggingPrivateStopFunction() = default;

 private:
  ~WebrtcLoggingPrivateStopFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStoreFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.store",
                             WEBRTCLOGGINGPRIVATE_STORE)
  WebrtcLoggingPrivateStoreFunction() = default;

 private:
  ~WebrtcLoggingPrivateStoreFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateUploadStoredFunction
    : public WebrtcLoggingPrivateFunctionWithUploadCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.uploadStored",
                             WEBRTCLOGGINGPRIVATE_UPLOADSTORED)
  WebrtcLoggingPrivateUploadStoredFunction() = default;

 private:
  ~WebrtcLoggingPrivateUploadStoredFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateUploadFunction
    : public WebrtcLoggingPrivateFunctionWithUploadCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.upload",
                             WEBRTCLOGGINGPRIVATE_UPLOAD)
  WebrtcLoggingPrivateUploadFunction() = default;

 private:
  ~WebrtcLoggingPrivateUploadFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateDiscardFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.discard",
                             WEBRTCLOGGINGPRIVATE_DISCARD)
  WebrtcLoggingPrivateDiscardFunction() = default;

 private:
  ~WebrtcLoggingPrivateDiscardFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStartRtpDumpFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.startRtpDump",
                             WEBRTCLOGGINGPRIVATE_STARTRTPDUMP)
  WebrtcLoggingPrivateStartRtpDumpFunction() = default;

 private:
  ~WebrtcLoggingPrivateStartRtpDumpFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStopRtpDumpFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.stopRtpDump",
                             WEBRTCLOGGINGPRIVATE_STOPRTPDUMP)
  WebrtcLoggingPrivateStopRtpDumpFunction() = default;

 private:
  ~WebrtcLoggingPrivateStopRtpDumpFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStartAudioDebugRecordingsFunction
    : public WebrtcLoggingPrivateFunctionWithRecordingDoneCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.startAudioDebugRecordings",
                             WEBRTCLOGGINGPRIVATE_STARTAUDIODEBUGRECORDINGS)
  WebrtcLoggingPrivateStartAudioDebugRecordingsFunction() = default;

 private:
  ~WebrtcLoggingPrivateStartAudioDebugRecordingsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStopAudioDebugRecordingsFunction
    : public WebrtcLoggingPrivateFunctionWithRecordingDoneCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.stopAudioDebugRecordings",
                             WEBRTCLOGGINGPRIVATE_STOPAUDIODEBUGRECORDINGS)
  WebrtcLoggingPrivateStopAudioDebugRecordingsFunction() = default;

 private:
  ~WebrtcLoggingPrivateStopAudioDebugRecordingsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class WebrtcLoggingPrivateStartEventLoggingFunction
    : public WebrtcLoggingPrivateFunctionWithGenericCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.startEventLogging",
                             WEBRTCLOGGINGPRIVATE_STARTEVENTLOGGING)
  WebrtcLoggingPrivateStartEventLoggingFunction() = default;

 private:
  ~WebrtcLoggingPrivateStartEventLoggingFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // If |success|, |log_id| must hold the ID. Otherwise, |error_message| must
  // hold a non-empty error message.
  // The function must be called on the UI thread.
  void FireCallback(bool success,
                    const std::string& log_id,
                    const std::string& error_message);
};

class WebrtcLoggingPrivateGetLogsDirectoryFunction
    : public WebrtcLoggingPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.getLogsDirectory",
                             WEBRTCLOGGINGPRIVATE_GETLOGSDIRECTORY)
  WebrtcLoggingPrivateGetLogsDirectoryFunction() = default;

 private:
  ~WebrtcLoggingPrivateGetLogsDirectoryFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Must be called on UI thread.
  void FireErrorCallback(const std::string& error_message);
  void FireCallback(const std::string& filesystem_id,
                    const std::string& base_name);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_LOGGING_PRIVATE_WEBRTC_LOGGING_PRIVATE_API_H_
