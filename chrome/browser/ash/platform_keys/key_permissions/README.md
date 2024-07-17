# Key Permissions

This directory contains code managing platform key permissions.

## Key Usages

This can only be “corporate” or undefined. If a key is marked for “corporate”
usage, only extensions listed in
[KeyPermissions](https://cloud.google.com/docs/chrome-enterprise/policies/?policy=KeyPermissions)
policy will be allowed to access this key via chrome.platformKeys and
chrome.enterprise.platformKeys APIs. Key Usages are considered to be
properties / metadata attached to keys themselves. This metadata was
historically persisted in a chromium Preference, but is now being migrated to
the backing key store (which is implemented by the chaps daemon on Chrome OS).

Usage of keys/certificates for network authentication and TLS client
authentication is currently not restricted by key usages, but this may change in
the future.

## Signing Permissions for Extensions

A (key, extension id) pair can have one of the following signing permissions:

* The key can be used once for signing. This permission is granted if an
extension generated the key using the enterprise.platformKeys API, so that it
can build a certification request.

* The key can not be used for signing. That will happen after an extension
generates a key using the enterprise.platformKeys API, and signs using it for
the first time to build a certification request.

* The key can be used for signing unlimited number of times. This permission is
granted by the user (only when the key is non-corporate and the profile is
non-managed) or the KeyPermissions policy to allow the extension to use the
key for signing through the
[enterprise.platformKeys](https://developer.chrome.com/extensions/enterprise.platformKeys)
or [platformKeys](https://developer.chrome.com/extensions/platformKeys) API.
