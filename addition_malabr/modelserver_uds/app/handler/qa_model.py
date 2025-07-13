from transformers import pipeline
from response import respond
import json
import socket

_qa_pipeline = None

def load_model(conn: socket.socket, payload: str) -> None:
    global _qa_pipeline
    if _qa_pipeline:
        respond(conn, "error", "Model already loaded.")
        return
    _qa_pipeline = pipeline(
        "question-answering",
        model="csarron/mobilebert-uncased-squad-v2",
        tokenizer="csarron/mobilebert-uncased-squad-v2"
    )
    respond(conn, "ok", "BERT model loaded.")

def infer(conn: socket.socket, payload: str) -> None:
    global _qa_pipeline
    if not _qa_pipeline:
        respond(conn, "error", "Model not loaded.")
        return
    try:
        data = json.loads(payload)
        question = data.get("question")
        context = data.get("context")
        if not question or not context:
            raise ValueError("Missing question or context")
        result = _qa_pipeline(question=question, context=context)
        respond(conn, "ok", "Answer: " + result["answer"])
    except Exception as e:
        respond(conn, "error", f"Inference failed: {e}")
