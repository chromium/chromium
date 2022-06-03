This directory contains the ash-chrome implementation of the ChromeOS API
(//chromeos/crosapi). This is the system implementation of ChromeOS-specific
functionality which lacros-chrome requires.

There are currently two types of files in this directory:
  * Files for launching and connecting to lacros-chrome. These are named
    lacros_foo.
  * Files that implement the crosapi. These are named foo_ash.
