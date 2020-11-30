# oobe-adaptive-dialog

## Overview

`oobe-adaptive-dialog` implements standard dialog which is used in OOBE screens.
This folder contains two different implementations of it. *old implementation is
just a fallback to old oobe-dialog which was used before.

## Usage

Both implementations have similar set of slots:

icon, title, progress (for progress bar), subtitle, content, back-navigation and
bottom-buttons.

back-navigation is usually used for holding back button, bottom-buttons for the
rest buttons.

It is intended to use `oobe-adaptive-dialog` along with style sheets:
`<style include="oobe-dialog-host"></style>`, and add `OobeDialogHostBehavior`.

Fonts and other styling for title, subtitle and regular text is applied
separately outside of the `oobe-adaptive-dialog` implementation.
