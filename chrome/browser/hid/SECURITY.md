# Chrome WebHID Chooser & Permission Context Security Notes

Supplemental security facts for `//chrome/browser/hid/*` (excluding test-only
code), supplementing
[services/device/SECURITY.md](/services/device/SECURITY.md) for general Web
Device APIs security, and Chromium's
[Security for Agents](/docs/security/security-for-agents.md). Before filing a
bug, review Security for Agents to double-check your findings.

## Threat Model & Problem Patterns

- **Per-Origin Permission Isolation & Hardware Inventory Integrity**: Validate
  non-empty device identifiers before persisting grants and never synthesize
  fake serial numbers for ephemeral devices. Scope ephemeral grants per
  `(origin, device)` while keeping the profile-wide connected hardware cache
  read-only during revocation so revoking one origin never evicts physical
  devices needed by other origins. Iterate all loaded `StoragePartition`s when
  cleaning up profile-wide connections.
- **Re-Entrancy on Permission Revocation & Chooser UI**: Complete all permission
  map updates before firing observer notifications (never notifying observers
  inside a `std::erase_if` predicate), and guard callers that store member state
  across synchronous chooser dialog creation with a `base::WeakPtr` check.
