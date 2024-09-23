This directory contains the interface to the application-bound encryption
primitives that are implemented by the elevation service in
[src/chrome/elevation_service].

`EncryptAppBoundString` and `DecryptAppBoundString` act like
`OSCrypt::EncryptString` and `OSCrypt::DecryptString` implemented by
[src/components/os_crypt] except that, unlike `OSCrypt`, which binds encrypted
data to the current user using DPAPI, this API will bind the encrypted data
with a `ProtectionLevel` specified by the caller.

`ProtectionLevels` are defined by chrome/elevation_service and are currently:

 - `ProtectionLevel::PROTECTION_NONE`

   This acts identically to DPAPI in that the protection level is user-bound.
   Only a `DecryptAppBoundString` call that comes from the same user principle
   as the original `EncryptAppBoundString` call with succeed.

 - `ProtectionLevel::PROTECTION_PATH_VALIDATION`

   This adds an additional protection that the path of the calling application
   will be validated. Only a `DecryptAppBoundString` call that comes from the
   same user principle, calling from the same Application (with the same file
   path) as the original `EncryptAppBoundString` call with succeed. It is only
   safe to call this from an application that is installed into a 'Trusted
   Path' such as `C:\Program Files`, otherwise protection can be trivially
   bypassed by renaming/placing a file into the required location.
