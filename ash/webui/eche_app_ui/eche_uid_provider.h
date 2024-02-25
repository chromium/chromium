// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_UID_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_UID_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

class PrefService;

namespace ash {
namespace eche_app {

extern const char kEcheAppSeedPref[];
extern const size_t kSeedSizeInByte;

// Implements the core logic of generating an UID for EcheApp and exposes the
// interface via mojo. Also store the UID in PrefService to persist UID.
class EcheUidProvider : public mojom::UidGenerator {
 public:
  explicit EcheUidProvider(PrefService* pref_service);
  ~EcheUidProvider() override;

  EcheUidProvider(const EcheUidProvider&) = delete;
  EcheUidProvider& operator=(const EcheUidProvider&) = delete;

  // mojom::UidGenerator:
  void GetUid(base::OnceCallback<void(const std::string&)> callback) override;

  void Bind(mojo::PendingReceiver<mojom::UidGenerator> receiver);

 private:
  friend class EcheUidProviderTest;

  std::string ConvertBinaryToString(base::span<const uint8_t> src);
  std::optional<std::vector<uint8_t>> ConvertStringToBinary(
      std::string_view str,
      size_t expected_len);
  void GenerateKeyPair(uint8_t public_key[ED25519_PUBLIC_KEY_LEN],
                       uint8_t private_key[ED25519_PRIVATE_KEY_LEN]);

  mojo::Receiver<mojom::UidGenerator> uid_receiver_{this};
  std::string uid_{};
  raw_ptr<PrefService> pref_service_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_UID_PROVIDER_H_
