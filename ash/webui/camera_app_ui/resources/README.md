# Camera App

Camera App is a packaged app designed to take photos and record videos.

## Supported systems

ChromeOS. Other platforms are not guaranteed to work.

## Installing, packaging, and testing

There is a helper script `utils/cca.py` with a convenient symlink `cca` in the
top directory to make some common tasks easier. For more details, please check
the usage of individual subcommands with the `--help` flag.

## Adding files

When adding a file (e.g. CSS/HTML/JS/Sound/Image), please also add the file name
into the list of corresponding .gni file. For example, when adding a "foo.js",
please also add "foo.js" into the list in "js/js.gni".

## Issues

* Issue Tracker: http://go/cca-buganizer
* File an issue: http://go/cca-newbug
