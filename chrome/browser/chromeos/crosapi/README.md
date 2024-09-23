Design doc: go/crosapi-for-ash-browser

This directory holds interfaces for the browser, running in either the Lacros or
Ash process, to access the Ash system over Crosapi. Headers in this directory
should be usable and consistent by both Lacros-browser and Ash-browser code
without the need for `#if BUILDFLAG(IS_CHROMEOS_LACROS)` or
`#if BUILDFLAG(IS_CHROMEOS_ASH)`.

Usage of this is restricted to browser code only, the Ash system must not use
the in-process crosapi IPC via mojo::Remote/Receiver to talk to itself.
Code using this directory must be compiled in Lacros.
