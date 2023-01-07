# Permission delegations

Permission delegation is a feature that allows the web content running in a Trusted Web Activities(TWA) client app making use of the client app’s Android  permission instead of checking Chrome’s permission setting for the website.

Currently permission delegation is implemented for notifications permission and location permission. For a Trusted Web Activity client app to support delegating any permissions, it must contain a TrustedWebActivityService.

## Notifications delegation

Whenever a package verifies for a web page's origin, we first check whether that package handles Intents for that web page's URL. Apps that verify for an origin but don’t handle Intents to it are ignored.

If the verified app can handle Intents, we perform notification delegation - resolving and connecting to the Trusted Web Activity Service - and query whether that app has the notification permission. On Android T+, the app may show a permission prompt to request runtime permission.

When Chrome displays notifications for the associated website, it will connect to the TrustedWebActivityService and pass the notification over. Notifications that outside of any TWA's scope will be displayed through Chrome.

## Location delegation

Location permission will only be granted when running as a TWA.

When a web content running in TWA trying to access geolocation, we will look for a TWA app that handles Intents for the site URL and query the TWA app’s Android permissions, and treat Android permissions to the corresponding ContentSettingValues.

If the location permission is granted, Chrome will connect to the TrustedWebActivityService and the client app should access the Android location API and provide the location data to Chrome.

If no TWA handle Intents for the site or the TWA did not declare the permission in its AndroidManifest, the location permission will decided by Chrome's site permission.