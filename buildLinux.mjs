// Script used by buildkite to build Chromium for Linux in CI
import path from "path";
import { spawnChecked } from "./replay_build_scripts/common.mjs";

const dockerArgs = [
  "run",
  "-e",
  "BUILDKITE",
  "-e",
  "BUILDKITE_BRANCH",
  "-e",
  "BUILDKITE_PIPELINE_DEFAULT_BRANCH",
  "-e",
  "LOCAL_DEVELOPER_BUILD_EXTENSION",
  "-e",
  "RBE_service",
  "-e",
  "RBE_service_no_auth",
  "-e",
  "RBE_use_application_default_credentials",
  "-e",
  "RBE_tls_client_auth_cert=/chromium/engflow.crt",
  "-e",
  "RBE_tls_client_auth_key=/chromium/engflow.key",
  "-e",
  "DRIVER_REVISION",
  "-v",
  `${path.join(process.env.HOME, "chromium")}:/chromium`,
  "-v",
  `${path.join(process.env.HOME, "depot_tools")}:/depot_tools`,
  "chromium-build-new",
];

spawnChecked("docker", dockerArgs, { stdio: "inherit" });
