# cc/mojom

[TOC]

## Overview

cc/mojom provides the mojom typemaps for cc types. This directory is where
cc types that appear as part of mojo APIs should be placed. Structures that
are defined in C++ and have mojo based NativeEnum definitions will
require legacy Chrome IPC validators, see cc/ipc.
