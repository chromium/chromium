# Running in Chrome

When a Trusted Web Activity (TWA) is launched to Chrome, it it shares Chrome's
Profile and storage partition.
This means that if you're logged into a website in Chrome, you're logged into
that website in the Trusted Web Activity.
To give the users an understanding of how their data is used and shared, we
must inform them of this.
(The behaviour described here is carried out independently for each TWA the
user installs and runs.)

## Chrome before version 84

From Chrome 72 (when TWAs were launched) to Chrome 84, we informed the users by
showing them a "Running in Chrome" Infobar.
This Infobar would be shown to the users the first time they launched the TWA
and would persist until they dismissed it.

## Chrome since version 84

Developers found the previous behavior intrusive and annoying, reducing the
amount of the screen that shows the webpage.
The new behavior now depends on whether the user has notifications for Chrome
enabled.

### If notifications are enabled

The first time a user opens a TWA in Chrome, they will get a silent, high
priority notification informing them that their data is being shared.
If the user taps on the notification or dismisses it, they won't trigger this
behavior again.
If the user does not interact with the notification, the next time they open a
TWA, they will get a similar notification, except it will now be low priority.

### If notifications are not enabled

The first time a user opens a TWA in Chrome, they will see a Snackbar informing
them that their data is shared with Chrome.
If the user interacts with the Snackbar, it is gone for good, if they do not,
it returns the next time the TWA is launched.

A Snackbar is different from an Infobar (what was shown before Chrome 84) in that a
Snackbar will dismiss itself after some time (in this case, 7 seconds) whereas
an Infobar will not.

## Code

The `TrustedWebActivityDisclosureController` determines whether a disclosure should be shown (eg, 
checking if a disclosure has already been accepted).
This state is pushed into the `TrustedWebActivityModel`.

One of three View classes will then read from the `TrustedWebActivityModel`, depending on which UI 
is appropriate:

* The `DisclosureNotification` if notifications are enabled.
* The `DisclosureSnackbar` if notifications are not enabled.
* The `DisclosureInfobar` containing the old behavior which is only kept around as a fall back in 
  case the new behavior breaks something.

The `DisclosureUiPicker` is responsible for choosing which View to instantiate.

## Links

* [Bug for the Running in Chrome v2 implementation.](https://crbug.com/1068106)