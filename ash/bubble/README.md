# //ash/bubble

Bubbles are widgets with rounded corners. They appear over the main workspace
content. Most bubbles are dismissed when the user clicks outside their bounds,
similar to a menu.

Example bubbles:

*   System tray (quick settings)
*   Phone hub
*   Bubble app list

This directory contains shared code for bubbles. Individual bubbles should be
implemented in other directories, like `//ash/app_list`, `//ash/system`, etc.
