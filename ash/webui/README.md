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

[1]: /ash/webui/BUILD.gn
