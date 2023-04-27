// Script used by buildkite to build Chromium for Windows in CI
import { spawnChecked, updateRepo } from "./replay_build_scripts/common.mjs";

updateRepo();

spawnChecked("node", ["build.js"], { stdio: "inherit" });
