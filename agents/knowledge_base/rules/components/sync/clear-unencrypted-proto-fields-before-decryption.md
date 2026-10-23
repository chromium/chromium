---
name: components/sync/clear-unencrypted-proto-fields-before-decryption
description: Use when processing incoming sync updates or deserializing protobuf messages with custom field-level encryption, such as an encrypted field paired with a plaintext counterpart.
rubric: safety
severity: should_fix
---

**Rule.** When processing incoming sync updates or deserializing protobuf
messages that use custom field-level encryption, explicitly clear the
corresponding unencrypted (plaintext) field before attempting decryption (and
ensure it remains cleared if decryption fails). Do not retain or trust plaintext
fields received on the wire when the protocol requires field encryption.

**Why.** If a remote client or server transmits unencrypted data in a field
intended to be encrypted, retaining the plaintext without decrypting bypasses
field-level encryption and accepts unauthenticated, untrusted payloads.
Explicitly clearing the plaintext field ensures that only legitimately decrypted
wire payloads populate local model entities.

## Examples

Bad:

```cpp
data.specifics = specifics;
if (data.specifics.send_tab_to_self().has_encrypted_page_context()) {
  if (!cryptographer->Decrypt(
          data.specifics.send_tab_to_self().encrypted_page_context(),
          data.specifics.mutable_send_tab_to_self()->mutable_page_context())) {
    return FAILED_TO_DECRYPT;
  }
  data.specifics.mutable_send_tab_to_self()->clear_encrypted_page_context();
}
```

Good:

```cpp
data.specifics = specifics;
// Clear unencrypted field so wire plaintext is ignored if received.
data.specifics.mutable_send_tab_to_self()->clear_page_context();

if (data.specifics.send_tab_to_self().has_encrypted_page_context()) {
  if (!cryptographer->Decrypt(
          data.specifics.send_tab_to_self().encrypted_page_context(),
          data.specifics.mutable_send_tab_to_self()->mutable_page_context())) {
    data.specifics.mutable_send_tab_to_self()->clear_page_context();
  }
  data.specifics.mutable_send_tab_to_self()->clear_encrypted_page_context();
}
```
