// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_
#define ASH_COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_

#include "ash/components/arc/mojom/audio.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcAudioBridge : public KeyedService,
                       public ConnectionObserver<mojom::AudioInstance>,
                       public mojom::AudioHost,
                       public ash::CrasAudioHandler::AudioObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAudioBridge* GetForBrowserContext(content::BrowserContext* context);
  static ArcAudioBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcAudioBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);

  ArcAudioBridge(const ArcAudioBridge&) = delete;
  ArcAudioBridge& operator=(const ArcAudioBridge&) = delete;

  ~ArcAudioBridge() override;

  // ConnectionObserver<mojom::AudioInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::AudioHost overrides.
  void ShowVolumeControls() override;
  void OnSystemVolumeUpdateRequest(int32_t percent) override;

  static void EnsureFactoryBuilt();

 private:
  // ash::CrasAudioHandler::AudioObserver overrides.
  void OnAudioNodesChanged() override;
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;

  void SendSwitchState(bool headphone_inserted, bool microphone_inserted);
  void SendVolumeState();

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  raw_ptr<ash::CrasAudioHandler, DanglingUntriaged> cras_audio_handler_;

  int volume_ = 0;  // Volume range: 0-100.
  bool muted_ = false;

  // Avoids sending requests when the instance is unavailable.
  // TODO(crbug.com/41213400): Remove once the root cause is fixed.
  bool available_ = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_
