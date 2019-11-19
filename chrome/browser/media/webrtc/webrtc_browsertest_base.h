// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_BASE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chrome/browser/media/webrtc/test_stats_dictionary.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace infobars {
class InfoBar;
}

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

// Base class for WebRTC browser tests with useful primitives for interacting
// getUserMedia. We use inheritance here because it makes the test code look
// as clean as it can be.
class WebRtcTestBase : public InProcessBrowserTest {
 public:
  // Typical constraints.
  static const char kAudioVideoCallConstraints[];
  static const char kAudioOnlyCallConstraints[];
  static const char kVideoOnlyCallConstraints[];
  static const char kVideoCallConstraintsQVGA[];
  static const char kVideoCallConstraints360p[];
  static const char kVideoCallConstraintsVGA[];
  static const char kVideoCallConstraints720p[];
  static const char kVideoCallConstraints1080p[];
  static const char kAudioVideoCallConstraints360p[];
  static const char kAudioVideoCallConstraints720p[];

  static const char kOkGotStream[];
  static const char kFailedWithNotAllowedError[];

  static const char kUseDefaultCertKeygen[];
  static const char kUseDefaultAudioCodec[];
  static const char kUseDefaultVideoCodec[];

  static const char kVP9Profile0Specifier[];
  static const char kVP9Profile2Specifier[];

  static const char kUndefined[];

  enum class StreamArgumentType {
    NO_STREAM,
    SHARED_STREAM,
    INDIVIDUAL_STREAMS
  };

 protected:
  WebRtcTestBase();
  ~WebRtcTestBase() override;

  // These all require that the loaded page fulfills the public interface in
  // chrome/test/data/webrtc/getusermedia.js.
  // If an error is reported back from the getUserMedia call, these functions
  // will return false.
  // The ...AndAccept()/...AndDeny()/...AndDismiss() functions expect that a
  // prompt will be shown (i.e. the current origin in the tab_contents doesn't
  // have a saved permission).
  bool GetUserMediaAndAccept(content::WebContents* tab_contents) const;
  bool GetUserMediaWithSpecificConstraintsAndAccept(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  bool GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  void GetUserMediaAndDeny(content::WebContents* tab_contents);
  void GetUserMediaWithSpecificConstraintsAndDeny(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  void GetUserMediaAndDismiss(content::WebContents* tab_contents) const;
  void GetUserMediaAndExpectAutoAcceptWithoutPrompt(
      content::WebContents* tab_contents) const;
  void GetUserMediaAndExpectAutoDenyWithoutPrompt(
      content::WebContents* tab_contents) const;
  void GetUserMedia(content::WebContents* tab_contents,
                    const std::string& constraints) const;

  // Convenience method which opens the page at url, calls GetUserMediaAndAccept
  // and returns the new tab.
  content::WebContents* OpenPageAndGetUserMediaInNewTab(const GURL& url) const;

  // Convenience method which opens the page at url, calls
  // GetUserMediaAndAcceptWithSpecificConstraints and returns the new tab.
  content::WebContents* OpenPageAndGetUserMediaInNewTabWithConstraints(
      const GURL& url, const std::string& constraints) const;

  // Convenience method which gets the URL for |test_page| and calls
  // OpenPageAndGetUserMediaInNewTab().
  content::WebContents* OpenTestPageAndGetUserMediaInNewTab(
    const std::string& test_page) const;

  // Convenience method which gets the URL for |test_page|, but without calling
  // GetUserMedia.
  content::WebContents* OpenTestPageInNewTab(
      const std::string& test_page) const;

  // Closes the last local stream acquired by the GetUserMedia* methods.
  void CloseLastLocalStream(content::WebContents* tab_contents) const;

  std::string ExecuteJavascript(const std::string& javascript,
                                content::WebContents* tab_contents) const;

  // TODO(https://crbug.com/1004239): Remove this function as soon as browser
  // tests stop relying on the legacy getStats() API.
  void ChangeToLegacyGetStats(content::WebContents* tab) const;

  // Sets up a peer connection in the tab and adds the current local stream
  // (which you can prepare by calling one of the GetUserMedia* methods above).
  // Optionally, |certificate_keygen_algorithm| is JavaScript for an
  // |AlgorithmIdentifier| to be used as parameter to
  // |RTCPeerConnection.generateCertificate|. The resulting certificate will be
  // used by the peer connection. Or use |kUseDefaultCertKeygen| to use a
  // certificate.
  void SetupPeerconnectionWithLocalStream(
      content::WebContents* tab,
      const std::string& certificate_keygen_algorithm =
          kUseDefaultCertKeygen) const;
  // Same as above but does not add the local stream.
  void SetupPeerconnectionWithoutLocalStream(
      content::WebContents* tab,
      const std::string& certificate_keygen_algorithm =
          kUseDefaultCertKeygen) const;
  // Same as |SetupPeerconnectionWithLocalStream| except a certificate is
  // specified, which is a reference to an |RTCCertificate| object.
  void SetupPeerconnectionWithCertificateAndLocalStream(
      content::WebContents* tab,
      const std::string& certificate) const;
  // Same as above but does not add the local stream.
  void SetupPeerconnectionWithCertificateWithoutLocalStream(
      content::WebContents* tab,
      const std::string& certificate) const;
  // Same as |SetupPeerconnectionWithLocalStream| except RTCPeerConnection
  // constraints are specified.
  void SetupPeerconnectionWithConstraintsAndLocalStream(
      content::WebContents* tab,
      const std::string& constraints,
      const std::string& certificate_keygen_algorithm =
          kUseDefaultCertKeygen) const;

  void CreateDataChannel(content::WebContents* tab, const std::string& label);

  // Exchanges offers and answers between the peer connections in the
  // respective tabs. Before calling this, you must have prepared peer
  // connections in both tabs and configured them as you like (for instance by
  // calling SetupPeerconnectionWithLocalStream).
  // If |video_codec| is not |kUseDefaultVideoCodec|, the SDP offer is modified
  // (and SDP answer verified) so that the specified video codec (case-sensitive
  // name) is used during the call instead of the default one.
  void NegotiateCall(content::WebContents* from_tab,
                     content::WebContents* to_tab) const;

  void VerifyLocalDescriptionContainsCertificate(
      content::WebContents* tab,
      const std::string& certificate) const;

  // Hangs up a negotiated call.
  void HangUp(content::WebContents* from_tab) const;

  // Call this to enable monitoring of javascript errors for this test method.
  // This will only work if the tests are run sequentially by the test runner
  // (i.e. with --test-launcher-developer-mode or --test-launcher-jobs=1).
  void DetectErrorsInJavaScript();

  // Methods for detecting if video is playing (the loaded page must have
  // chrome/test/data/webrtc/video_detector.js and its dependencies loaded to
  // make that work). Looks at a 320x240 area of the target video tag.
  void StartDetectingVideo(content::WebContents* tab_contents,
                           const std::string& video_element) const;
  bool WaitForVideoToPlay(content::WebContents* tab_contents) const;

  // Returns the stream size as a string on the format <width>x<height>.
  std::string GetStreamSize(content::WebContents* tab_contents,
                            const std::string& video_element) const;

  // Returns true if we're on Windows 8 or higher.
  bool OnWin8OrHigher() const;

  void OpenDatabase(content::WebContents* tab) const;
  void CloseDatabase(content::WebContents* tab) const;
  void DeleteDatabase(content::WebContents* tab) const;

  void GenerateAndCloneCertificate(content::WebContents* tab,
                                   const std::string& keygen_algorithm) const;

  void VerifyStatsGeneratedCallback(content::WebContents* tab) const;
  double MeasureGetStatsCallbackPerformance(content::WebContents* tab) const;
  std::vector<std::string> VerifyStatsGeneratedPromise(
      content::WebContents* tab) const;
  scoped_refptr<content::TestStatsReportDictionary> GetStatsReportDictionary(
      content::WebContents* tab) const;
  double MeasureGetStatsPerformance(content::WebContents* tab) const;
  std::vector<std::string> GetMandatoryStatsTypes(
      content::WebContents* tab) const;

  // Change the default audio/video codec in the offer SDP.
  void SetDefaultAudioCodec(content::WebContents* tab,
                            const std::string& audio_codec) const;
  // |prefer_hw_codec| controls what codec with name |video_codec| (and with
  // profile |profile| if given)should be selected. This parameter only matters
  // if there are multiple codecs with the same name, which can be the case for
  // H264.
  void SetDefaultVideoCodec(content::WebContents* tab,
                            const std::string& video_codec,
                            bool prefer_hw_codec = false,
                            const std::string& profile = std::string()) const;

  // Add 'usedtx=1' to the offer SDP.
  void EnableOpusDtx(content::WebContents* tab) const;

  // Try to open a dekstop media stream, and return the stream id.
  // On failure, will return empty string.
  std::string GetDesktopMediaStream(content::WebContents* tab);
  base::Optional<std::string> LoadDesktopCaptureExtension();

 private:
  void CloseInfoBarInTab(content::WebContents* tab_contents,
                         infobars::InfoBar* infobar) const;

  std::string CreateLocalOffer(content::WebContents* from_tab) const;
  std::string CreateAnswer(std::string local_offer,
                           content::WebContents* to_tab) const;
  void ReceiveAnswer(const std::string& answer,
                     content::WebContents* from_tab) const;
  void GatherAndSendIceCandidates(content::WebContents* from_tab,
                                  content::WebContents* to_tab) const;
  bool HasStreamWithTrack(content::WebContents* tab,
                          const char* function_name,
                          std::string stream_id,
                          std::string track_id) const;

  infobars::InfoBar* GetUserMediaAndWaitForInfoBar(
      content::WebContents* tab_contents,
      const std::string& constraints) const;

  bool detect_errors_in_javascript_;
  scoped_refptr<const extensions::Extension> desktop_capture_extension_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcTestBase);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_BASE_H_
