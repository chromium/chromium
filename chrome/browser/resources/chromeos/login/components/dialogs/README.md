# OOBE Dialogs

## oobe-adaptive-dialog

### Overview

`oobe-adaptive-dialog` implements standard dialog which is used in OOBE screens.
This folder contains two different implementations of it. *old implementation is
just a fallback to old oobe-dialog which was used before.

### Usage

Both implementations have similar set of slots:

icon, title, progress (for progress bar), subtitle, content, back-navigation and
bottom-buttons.

back-navigation is usually used for holding back button, bottom-buttons for the
rest buttons.

It is intended to use `oobe-adaptive-dialog` along with style sheets:
`<style include="oobe-dialog-host-styles"></style>`, and add `OobeDialogHostBehavior`.

Fonts and other styling for title, subtitle and regular text is applied
separately outside of the `oobe-adaptive-dialog` implementation.

## oobe-content-dialog

### Overview

`oobe-content-dialog` implements dialog for content placing only. It has one
mode for landscape and portrait orientation. This folder contains two different
implementations of it. Old implementation is just a fallback to old oobe-dialog
which was used before.

### Usage

Both implementations have similar set of slots:
`content`, `back-navigation` and `bottom-buttons`.

back-navigation is usually used for holding back button, bottom-buttons for the
rest buttons.

It is intended to use `oobe-content-dialog` along with style sheets:
`<style include="oobe-dialog-host-styles"></style>`, and add `OobeDialogHostBehavior`.

Most of the uses of `oobe-content-dialog` are hosting `webview` which usually
has its own title, subtitle and icon.
If there is an intention to show content without buttons it is possible to leave
button slots empty - dialog still would have proper internal sizing and place
the content properly. An example of such use is showing `throbber-notice`.
