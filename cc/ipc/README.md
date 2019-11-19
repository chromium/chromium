# cc/ipc

[TOC]

## Overview

cc/ipc provides Chrome IPC legacy param trait validators. cc based
structures that are defined in C++ and have mojo based NativeEnum
definitions require validators. See cc/mojom for the mojo definitions.
Eventually all cc based structures should be defined solely in
mojo and then this directory can be removed. However, this will
not happen until all structures are sent via mojo only.
