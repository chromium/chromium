# //ash/webui

//ash/webui contains code that is Chrome OS-specific WebUI for system web
apps and has dependencies on //content.

General purpose window manager or system UI code should not have content
dependencies, and should not live in this directory. Prefer a different
top-level ash directory, like //ash/system, //ash/wm, or add
//ash/your_feature. Low-level components go in //ash/components/your_feature.

Each subdirectory should be its own separate "module", and have its own
BUILD.gn file. See this directory's [BUILD.gn file][1] for tips on adding
your own subdirectory.

This directory is in //ash because it runs in the "ash-chrome" binary when
Lacros is running. Most of its subdirectories used to live in
//chromeos/components. See the [Lacros documentation][2] or the Lacros
[directory migration design][3].

[1]: /ash/webui/BUILD.gn
[2]: /docs/lacros.md
[3]: https://docs.google.com/document/d/1g-98HpzA8XcoGBWUv1gQNr4rbnD5yfvbtYZyPDDbkaE/edit#heading=h.5aq0kntd3afh
