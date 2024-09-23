// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_CHAPS_SESSION_CHAPS_CLIENT_H_
#define ASH_COMPONENTS_KCER_CHAPS_SESSION_CHAPS_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "ash/components/kcer/attributes.pb.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/strong_alias.h"
#include "chromeos/ash/components/dbus/chaps/chaps_client.h"
#include "chromeos/crosapi/mojom/chaps_service.mojom.h"

namespace kcer {

// Opens PKCS#11 sessions that are required for most requests to Chaps. Opens a
// new session when there's none for the required slot at the beginning of each
// public method. `attempts_left` determines how many attempts the method has
// for opening a session. On failing to open a session the method will return
// `chaps::kFailedToOpenSessionError`. If there was an open session at the
// beginning of the method, but it got closed, one of the
// PKCS11_CKR_SESSION_HANDLE_INVALID, PKCS11_CKR_SESSION_CLOSED errors will be
// returned and the caller is expected to discard all cached object handles and
// try again (potentially repeating some earlier requests).
// Exported for unit tests and KcerFactory only.
class COMPONENT_EXPORT(KCER) SessionChapsClient {
 public:
  // Strong alias for some types to prevent incorrect cross-assignment.
  using SlotId = base::StrongAlias<class TypeTagSlotId, uint64_t>;
  using SessionId = base::StrongAlias<class TypeTagSessionId, uint64_t>;
  using ObjectHandle = base::StrongAlias<class TypeTagObjectHandle, uint64_t>;

  using GetMechanismListCallback =
      base::OnceCallback<void(const std::vector<uint64_t>& mechanism_list,
                              uint32_t result_code)>;
  using CreateObjectCallback =
      base::OnceCallback<void(ObjectHandle object_handle,
                              uint32_t result_code)>;
  using DestroyObjectCallback = base::OnceCallback<void(uint32_t result_code)>;
  using GetAttributeValueCallback =
      base::OnceCallback<void(std::vector<uint8_t> attributes,
                              uint32_t result_code)>;
  using SetAttributeValueCallback =
      base::OnceCallback<void(uint32_t result_code)>;
  using FindObjectsCallback =
      base::OnceCallback<void(std::vector<ObjectHandle> object_list,
                              uint32_t result_code)>;
  using SignCallback = base::OnceCallback<void(std::vector<uint8_t> signature,
                                               uint32_t result_code)>;
  using GenerateKeyPairCallback =
      base::OnceCallback<void(ObjectHandle public_key_handle,
                              ObjectHandle private_key_handle,
                              uint32_t result_code)>;

  SessionChapsClient();
  virtual ~SessionChapsClient();

  // Returns true if the `result_code` contains an error related to
  // problems with PKCS#11 session, i.e. if the session cannot be used
  // anymore. In most cases it means that all existing ObjectHandler-s
  // should be discarded and the operation should be retried.
  static bool IsSessionError(uint32_t result_code);

  // A convenience method for serializing `chaps::AttributeList`.
  static std::vector<uint8_t> SerializeToBytes(
      const chaps::AttributeList& attr_list);

  // PKCS #11 v2.20 section 11.5 page 111.
  virtual void GetMechanismList(SlotId slot_id,
                                GetMechanismListCallback callback) = 0;

  // PKCS #11 v2.20 section 11.7 page 128.
  virtual void CreateObject(SlotId slot_id,
                            // Serialized chaps::AttributeList.
                            const std::vector<uint8_t>& attributes,
                            int attempts_left,
                            CreateObjectCallback callback) = 0;

  // PKCS #11 v2.20 section 11.7 page 131.
  virtual void DestroyObject(SlotId slot_id,
                             ObjectHandle object_handle,
                             int attempts_left,
                             DestroyObjectCallback callback) = 0;

  // PKCS #11 v2.20 section 11.7 page 133.
  virtual void GetAttributeValue(SlotId slot_id,
                                 ObjectHandle object_handle,
                                 // Serialized chaps::AttributeList.
                                 std::vector<uint8_t> attributes_query,
                                 int attempts_left,
                                 GetAttributeValueCallback callback) = 0;

  // PKCS #11 v2.20 section 11.7 page 135.
  virtual void SetAttributeValue(SlotId slot_id,
                                 ObjectHandle object_handle,
                                 // Serialized chaps::AttributeList.
                                 std::vector<uint8_t> attributes,
                                 int attempts_left,
                                 SetAttributeValueCallback callback) = 0;

  // Combines FindObjects* methods, PKCS #11 v2.20 section 11.7 page
  // 136-138. Can return up to the `kFindObjectsMaxCount` object handles.
  virtual void FindObjects(SlotId slot_id,
                           // Serialized chaps::AttributeList.
                           std::vector<uint8_t> attributes,
                           int attempts_left,
                           FindObjectsCallback callback) = 0;

  // Combines SignInit and Sign, PKCS #11 v2.20 section 11.7 page 152-153.
  virtual void Sign(SlotId slot_id,
                    uint64_t mechanism_type,
                    std::vector<uint8_t> mechanism_parameter,
                    ObjectHandle key_handle,
                    std::vector<uint8_t> data,
                    int attempts_left,
                    SignCallback callback) = 0;

  // PKCS #11 v2.20 section 11.14 page 176.
  virtual void GenerateKeyPair(SlotId slot_id,
                               uint64_t mechanism_type,
                               // Serialized chaps::AttributeList-s.
                               std::vector<uint8_t> mechanism_parameter,
                               std::vector<uint8_t> public_key_attributes,
                               std::vector<uint8_t> private_key_attributes,
                               int attempts_left,
                               GenerateKeyPairCallback callback) = 0;
};

// Exported for unit tests and KcerFactory only.
class COMPONENT_EXPORT(KCER) SessionChapsClientImpl
    : public SessionChapsClient {
 public:
  explicit SessionChapsClientImpl();
  ~SessionChapsClientImpl() override;

  // Implements SessionChapsClient.
  void GetMechanismList(SlotId slot_id,
                        GetMechanismListCallback callback) override;
  void CreateObject(SlotId slot_id,
                    const std::vector<uint8_t>& attributes,
                    int attempts_left,
                    CreateObjectCallback callback) override;
  void DestroyObject(SlotId slot_id,
                     ObjectHandle object_handle,
                     int attempts_left,
                     DestroyObjectCallback callback) override;
  void GetAttributeValue(SlotId slot_id,
                         ObjectHandle object_handle,
                         // Serialized chaps::AttributeList.
                         std::vector<uint8_t> attributes_query,
                         int attempts_left,
                         GetAttributeValueCallback callback) override;
  void SetAttributeValue(SlotId slot_id,
                         ObjectHandle object_handle,
                         // Serialized chaps::AttributeList.
                         std::vector<uint8_t> attributes,
                         int attempts_left,
                         SetAttributeValueCallback callback) override;
  void FindObjects(SlotId slot_id,
                   // Serialized chaps::AttributeList.
                   std::vector<uint8_t> attributes,
                   int attempts_left,
                   FindObjectsCallback callback) override;
  void Sign(SlotId slot_id,
            uint64_t mechanism_type,
            std::vector<uint8_t> mechanism_parameter,
            ObjectHandle key_handle,
            std::vector<uint8_t> data,
            int attempts_left,
            SignCallback callback) override;
  void GenerateKeyPair(SlotId slot_id,
                       uint64_t mechanism_type,
                       // Serialized chaps::AttributeList-s.
                       std::vector<uint8_t> mechanism_parameter,
                       std::vector<uint8_t> public_attributes,
                       std::vector<uint8_t> private_attributes,
                       int attempts_left,
                       GenerateKeyPairCallback callback) override;

 private:
  void DidCreateObject(SlotId slot_id,
                       CreateObjectCallback callback,
                       uint64_t object_handle,
                       uint32_t result_code);
  void DidDestroyObject(SlotId slot_id,
                        DestroyObjectCallback callback,
                        uint32_t result_code);
  void DidGetAttributeValue(SlotId slot_id,
                            GetAttributeValueCallback callback,
                            const std::vector<uint8_t>& attributes,
                            uint32_t result_code);
  void DidSetAttributeValue(SlotId slot_id,
                            SetAttributeValueCallback callback,
                            uint32_t result_code);

  void DidFindObjectsInit(SlotId slot_id,
                          FindObjectsCallback callback,
                          uint32_t result_code);
  void DidFindObjects(SlotId slot_id,
                      FindObjectsCallback callback,
                      const std::vector<uint64_t>& object_list,
                      uint32_t result_code);
  void DidFindObjectsFinal(SlotId slot_id,
                           FindObjectsCallback callback,
                           std::vector<ObjectHandle> object_list,
                           uint32_t result_code);
  void DidSignInit(SlotId slot_id,
                   std::vector<uint8_t> data,
                   SignCallback callback,
                   uint32_t result_code);
  void DidSign(SlotId slot_id,
               SignCallback callback,
               uint64_t actual_out_length,
               const std::vector<uint8_t>& signature,
               uint32_t result_code);
  void DidGenerateKeyPair(SlotId slot_id,
                          GenerateKeyPairCallback callback,
                          uint64_t public_key_id,
                          uint64_t private_key_id,
                          uint32_t result_code);

  void SaveSessionId(SlotId slot_id,
                     base::OnceClosure callback,
                     uint64_t session_id,
                     uint32_t result_code);

  SessionId GetSessionForSlot(SlotId slot_id) const;

  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_map<SlotId, SessionId> sessions_map_;
  base::WeakPtrFactory<SessionChapsClientImpl> weak_factory_{this};
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_CHAPS_SESSION_CHAPS_CLIENT_H_
