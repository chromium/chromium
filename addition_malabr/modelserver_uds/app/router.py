from types_defs import Payload
import socket
from handler import read_data, send_data, qa_model

def route(conn: socket.socket, payload: Payload):
    label = payload["label"]
    raw_payload = payload["payload"]

    ROUTES = {
        "LABEL_READ_DATA": read_data.handle,
        "LABEL_SEND_DATA": send_data.handle,
        "LABEL_LOAD_MODEL_BERT": qa_model.load_model,
        "LABEL_INFER_MODEL_BERT": qa_model.infer
    }

    handler = ROUTES.get(label)
    if handler:
        print(f"[HIT]: {label}")
        handler(conn, raw_payload)
    else:
        from response import respond
        respond(conn, "error", f"Unknown label: {label}")
