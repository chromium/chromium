# Quick Insert

Quick Insert is a feature in Ash that allows users to quickly insert
emojis, GIFs, links, images, and more. It is triggered via a dedicated
key on the keyboard or a keyboard shortcut. The user can search for
something in the Quick Insert window and insert it directly without
leaving the input field.

## Key Components

* `model/`: The data model to be rendered.
* `views/`: The UI related code.
* `metrics/`: Code for recording metrics.
* `PickerController`: Controls the visibility of the Picker.
* `PickerClient`: Used by `PickerController` to talk to the browser.
