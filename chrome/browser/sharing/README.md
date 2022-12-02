This directory contains features for sharing data between one user's Chrome
instances: click-to-call, shared clipboard, and so on. Social features for
sharing between users go instead in //chrome/browser/share. The features in this
directory are backed by an [FCM] (Firebase Messaging) service and can optionally
use [VAPID].

If you would like to use this machinery, please contact groby@, markchang@, or
ellyjones@.

[FCM]: https://firebase.google.com/docs/cloud-messaging
[VAPID]: https://datatracker.ietf.org/doc/html/draft-thomson-webpush-vapid-02
